// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"

#include <unordered_set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/cookie_store/cookie_store_manager.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_tracker.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

// These constants are used to create the directory structure under the profile
// where renderers with a non-default storage partition keep their persistent
// state. This will contain a set of directories that partially mirror the
// directory structure of BrowserContext::GetPath().
//
// The kStoragePartitionDirname contains an extensions directory which is
// further partitioned by extension id, followed by another level of directories
// for the "default" extension storage partition and one directory for each
// persistent partition used by a webview tag. Example:
//
//   Storage/ext/ABCDEF/def
//   Storage/ext/ABCDEF/hash(partition name)
//
// The code in GetStoragePartitionPath() constructs these path names.
//
// TODO(nasko): Move extension related path code out of content.
const base::FilePath::CharType kStoragePartitionDirname[] =
    FILE_PATH_LITERAL("Storage");
const base::FilePath::CharType kExtensionsDirname[] =
    FILE_PATH_LITERAL("ext");
const base::FilePath::CharType kDefaultPartitionDirname[] =
    FILE_PATH_LITERAL("def");
const base::FilePath::CharType kTrashDirname[] =
    FILE_PATH_LITERAL("trash");

// Because partition names are user specified, they can be arbitrarily long
// which makes them unsuitable for paths names. We use a truncation of a
// SHA256 hash to perform a deterministic shortening of the string. The
// kPartitionNameHashBytes constant controls the length of the truncation.
// We use 6 bytes, which gives us 99.999% reliability against collisions over
// 1 million partition domains.
//
// Analysis:
// We assume that all partition names within one partition domain are
// controlled by the the same entity. Thus there is no chance for adverserial
// attack and all we care about is accidental collision. To get 5 9s over
// 1 million domains, we need the probability of a collision in any one domain
// to be
//
//    p < nroot(1000000, .99999) ~= 10^-11
//
// We use the following birthday attack approximation to calculate the max
// number of unique names for this probability:
//
//    n(p,H) = sqrt(2*H * ln(1/(1-p)))
//
// For a 6-byte hash, H = 2^(6*8).  n(10^-11, H) ~= 75
//
// An average partition domain is likely to have less than 10 unique
// partition names which is far lower than 75.
//
// Note, that for 4 9s of reliability, the limit is 237 partition names per
// partition domain.
const int kPartitionNameHashBytes = 6;

// Needed for selecting all files in ObliterateOneDirectory() below.
#if BUILDFLAG(IS_POSIX)
const int kAllFileTypes = base::FileEnumerator::FILES |
                          base::FileEnumerator::DIRECTORIES |
                          base::FileEnumerator::SHOW_SYM_LINKS;
#else
const int kAllFileTypes = base::FileEnumerator::FILES |
                          base::FileEnumerator::DIRECTORIES;
#endif

base::FilePath GetStoragePartitionDomainPath(
    const std::string& partition_domain) {
  CHECK(base::IsStringUTF8(partition_domain));

  return base::FilePath(kStoragePartitionDirname).Append(kExtensionsDirname)
      .Append(base::FilePath::FromUTF8Unsafe(partition_domain));
}

// Helper function for doing a depth-first deletion of the data on disk.
// Examines paths directly in |current_dir| (no recursion) and tries to
// delete from disk anything that is in, or isn't a parent of something in
// |paths_to_keep|. Paths that need further expansion are added to
// |paths_to_consider|.
void ObliterateOneDirectory(const base::FilePath& current_dir,
                            const std::vector<base::FilePath>& paths_to_keep,
                            std::vector<base::FilePath>* paths_to_consider) {
  CHECK(current_dir.IsAbsolute());

  base::FileEnumerator enumerator(current_dir, false, kAllFileTypes);
  for (base::FilePath to_delete = enumerator.Next(); !to_delete.empty();
       to_delete = enumerator.Next()) {
    // Enum tracking which of the 3 possible actions to take for |to_delete|.
    enum { kSkip, kEnqueue, kDelete } action = kDelete;

    for (auto to_keep = paths_to_keep.begin(); to_keep != paths_to_keep.end();
         ++to_keep) {
      if (to_delete == *to_keep) {
        action = kSkip;
        break;
      } else if (to_delete.IsParent(*to_keep)) {
        // |to_delete| contains a path to keep. Add to stack for further
        // processing.
        action = kEnqueue;
        break;
      }
    }

    switch (action) {
      case kDelete:
        base::DeletePathRecursively(to_delete);
        break;

      case kEnqueue:
        paths_to_consider->push_back(to_delete);
        break;

      case kSkip:
        break;
    }
  }
}

// Synchronously attempts to delete |unnormalized_root|, preserving only
// entries in |paths_to_keep|. If there are no entries in |paths_to_keep| on
// disk, then it completely removes |unnormalized_root|. All paths must be
// absolute paths.
void BlockingObliteratePath(
    const base::FilePath& unnormalized_browser_context_root,
    const base::FilePath& unnormalized_root,
    const std::vector<base::FilePath>& paths_to_keep,
    const scoped_refptr<base::TaskRunner>& closure_runner,
    base::OnceClosure on_gc_required) {
  // Early exit required because MakeAbsoluteFilePath() will fail on POSIX
  // if |unnormalized_root| does not exist. This is safe because there is
  // nothing to do in this situation anwyays.
  if (!base::PathExists(unnormalized_root)) {
    return;
  }

  // Never try to obliterate things outside of the browser context root or the
  // browser context root itself. Die hard.
  base::FilePath root = base::MakeAbsoluteFilePath(unnormalized_root);
  base::FilePath browser_context_root =
      base::MakeAbsoluteFilePath(unnormalized_browser_context_root);
  CHECK(!root.empty());
  CHECK(!browser_context_root.empty());
  CHECK(browser_context_root.IsParent(root) && browser_context_root != root);

  // Reduce |paths_to_keep| set to those under the root and actually on disk.
  std::vector<base::FilePath> valid_paths_to_keep;
  for (auto it = paths_to_keep.begin(); it != paths_to_keep.end(); ++it) {
    if (root.IsParent(*it) && base::PathExists(*it))
      valid_paths_to_keep.push_back(*it);
  }

  // If none of the |paths_to_keep| are valid anymore then we just whack the
  // root and be done with it.  Otherwise, signal garbage collection and do
  // a best-effort delete of the on-disk structures.
  if (valid_paths_to_keep.empty()) {
    base::DeletePathRecursively(root);
    return;
  }
  closure_runner->PostTask(FROM_HERE, std::move(on_gc_required));

  // Otherwise, start at the root and delete everything that is not in
  // |valid_paths_to_keep|.
  std::vector<base::FilePath> paths_to_consider;
  paths_to_consider.push_back(root);
  while(!paths_to_consider.empty()) {
    base::FilePath path = paths_to_consider.back();
    paths_to_consider.pop_back();
    ObliterateOneDirectory(path, valid_paths_to_keep, &paths_to_consider);
  }
}

// Ensures each path in |active_paths| is a direct child of storage_root.
void NormalizeActivePaths(const base::FilePath& storage_root,
                          std::unordered_set<base::FilePath>* active_paths) {
  std::unordered_set<base::FilePath> normalized_active_paths;

  for (auto iter = active_paths->begin(); iter != active_paths->end(); ++iter) {
    base::FilePath relative_path;
    if (!storage_root.AppendRelativePath(*iter, &relative_path))
      continue;

    std::vector<base::FilePath::StringType> components =
        relative_path.GetComponents();

    DCHECK(!relative_path.empty());
    normalized_active_paths.insert(storage_root.Append(components.front()));
  }

  active_paths->swap(normalized_active_paths);
}

// Deletes all entries inside the |storage_root| that are not in the
// |active_paths|.  Deletion is done in 2 steps:
//
//   (1) Moving all garbage collected paths into a trash directory.
//   (2) Asynchronously deleting the trash directory.
//
// The deletion is asynchronous because after (1) completes, calling code can
// safely continue to use the paths that had just been garbage collected
// without fear of race conditions.
//
// This code also ignores failed moves rather than attempting a smarter retry.
// Moves shouldn't fail here unless there is some out-of-band error (eg.,
// FS corruption). Retry logic is dangerous in the general case because
// there is not necessarily a guaranteed case where the logic may succeed.
//
// This function is still named BlockingGarbageCollect() because it does
// execute a few filesystem operations synchronously.
void BlockingGarbageCollect(
    const base::FilePath& storage_root,
    const scoped_refptr<base::TaskRunner>& file_access_runner,
    std::unordered_set<base::FilePath> active_paths) {
  CHECK(storage_root.IsAbsolute());

  NormalizeActivePaths(storage_root, &active_paths);

  base::FileEnumerator enumerator(storage_root, false, kAllFileTypes);
  base::FilePath trash_directory;
  if (!base::CreateTemporaryDirInDir(storage_root, kTrashDirname,
                                     &trash_directory)) {
    // Unable to continue without creating the trash directory so give up.
    return;
  }
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (!base::Contains(active_paths, path) && path != trash_directory) {
      // Since |trash_directory| is unique for each run of this function there
      // can be no colllisions on the move.
      base::Move(path, trash_directory.Append(path.BaseName()));
    }
  }

  file_access_runner->PostTask(
      FROM_HERE, base::GetDeletePathRecursivelyCallback(trash_directory));
}

}  // namespace

// static
base::FilePath StoragePartitionImplMap::GetStoragePartitionPath(
    const std::string& partition_domain,
    const std::string& partition_name) {
  if (partition_domain.empty())
    return base::FilePath();

  base::FilePath path = GetStoragePartitionDomainPath(partition_domain);

  // TODO(ajwong): Mangle in-memory into this somehow, either by putting
  // it into the partition_name, or by manually adding another path component
  // here.  Otherwise, it's possible to have an in-memory StoragePartition and
  // a persistent one that return the same FilePath for GetPath().
  if (!partition_name.empty()) {
    // For analysis of why we can ignore collisions, see the comment above
    // kPartitionNameHashBytes.
    uint8_t buffer[kPartitionNameHashBytes];
    crypto::SHA256HashString(partition_name, buffer, sizeof(buffer));
    return path.AppendASCII(base::HexEncode(buffer));
  }

  return path.Append(kDefaultPartitionDirname);
}

StoragePartitionImplMap::StoragePartitionImplMap(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      file_access_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      resource_context_initialized_(false) {}

StoragePartitionImplMap::~StoragePartitionImplMap() {
}

StoragePartitionImpl* StoragePartitionImplMap::Get(
    const StoragePartitionConfig& partition_config,
    bool can_create) {
  // Find the previously created partition if it's available.
  if (auto* partition = base::FindPtrOrNull(partitions_, partition_config)) {
    return partition;
  }

  if (!can_create)
    return nullptr;

  base::FilePath relative_partition_path = GetStoragePartitionPath(
      partition_config.partition_domain(), partition_config.partition_name());

  std::optional<StoragePartitionConfig> fallback_config =
      partition_config.GetFallbackForBlobUrls();
  StoragePartitionImpl* fallback_for_blob_urls =
      fallback_config.has_value() ? Get(*fallback_config, /*can_create=*/false)
                                  : nullptr;

  std::unique_ptr<StoragePartitionImpl> partition_ptr(
      StoragePartitionImpl::Create(browser_context_, partition_config,
                                   relative_partition_path));
  StoragePartitionImpl* partition = partition_ptr.get();
  partitions_[partition_config] = std::move(partition_ptr);
  partition->Initialize(fallback_for_blob_urls);

  // Arm the serviceworker cookie change observation API.
  partition->GetCookieStoreManager()->ListenToCookieChanges(
      partition->GetNetworkContext(), base::DoNothing());

  PostCreateInitialization(partition, partition_config.in_memory());

  return partition;
}

void StoragePartitionImplMap::AsyncObliterate(
    const std::string& partition_domain,
    base::OnceClosure on_gc_required,
    base::OnceClosure done_callback) {
  // Find the active partitions for the domain. Because these partitions are
  // active, it is not possible to just delete the directories that contain
  // the backing data structures without causing the browser to crash. Instead,
  // of deleteing the directory, we tell each storage context later to
  // remove any data they have saved. This will leave the directory structure
  // intact but it will only contain empty databases.
  std::vector<StoragePartitionImpl*> active_partitions;
  std::vector<base::FilePath> paths_to_keep;
  for (PartitionMap::const_iterator it = partitions_.begin();
       it != partitions_.end();
       ++it) {
    const StoragePartitionConfig& config = it->first;
    if (config.partition_domain() == partition_domain) {
      active_partitions.push_back(it->second.get());
      if (!config.in_memory()) {
        paths_to_keep.push_back(it->second->GetPath());
      }
    }
  }

  // Create a barrier closure for keeping track of the callbacks in
  // AsyncObliterate(). We have one callback for each active partition that is
  // cleared and an additional one for BlockingObliteratePath()'s task reply.
  int num_tasks = active_partitions.size() + 1;
  auto subtask_done_callback =
      base::BarrierClosure(num_tasks, std::move(done_callback));

  for (auto*& active_partition : active_partitions) {
    active_partition->ClearData(
        // All except shader cache.
        ~StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
        StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, blink::StorageKey(),
        base::Time(), base::Time::Max(), subtask_done_callback);
  }

  // Start a best-effort delete of the on-disk storage excluding paths that are
  // known to still be in use. This is to delete any previously created
  // StoragePartition state that just happens to not have been used during this
  // run of the browser.
  base::FilePath domain_root = browser_context_->GetPath().Append(
      GetStoragePartitionDomainPath(partition_domain));

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BlockingObliteratePath, browser_context_->GetPath(),
                     domain_root, paths_to_keep,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(on_gc_required)),
      subtask_done_callback);
}

void StoragePartitionImplMap::GarbageCollect(
    std::unordered_set<base::FilePath> active_paths,
    base::OnceClosure done) {
  // Include all paths for current StoragePartitions in the active_paths since
  // they cannot be deleted safely.
  for (const auto& part : partitions_) {
    const StoragePartitionConfig& config = part.first;
    if (!config.in_memory())
      active_paths.insert(part.second->GetPath());
  }

  // Find the directory holding the StoragePartitions and delete everything in
  // there that isn't considered active.
  base::FilePath storage_root = browser_context_->GetPath().Append(
      GetStoragePartitionDomainPath(std::string()));
  file_access_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&BlockingGarbageCollect, storage_root, file_access_runner_,
                     std::move(active_paths)),
      std::move(done));
}

void StoragePartitionImplMap::ForEach(
    base::FunctionRef<void(StoragePartition*)> fn) {
  for (const auto& [config, partition] : partitions_) {
    fn(partition.get());
  }
}

void StoragePartitionImplMap::PostCreateInitialization(
    StoragePartitionImpl* partition,
    bool in_memory) {
  // TODO(ajwong): ResourceContexts no longer have any storage related state.
  // We should move this into a place where it is called once per
  // BrowserContext creation rather than piggybacking off the default context
  // creation.
  // Note: moving this into Get() before partitions_[] is set causes reentrency.
  if (!resource_context_initialized_) {
    resource_context_initialized_ = true;
    InitializeResourceContext(browser_context_);
  }

#if !BUILDFLAG(IS_ANDROID)
  if (!in_memory) {
    // Clean up any lingering WebSQL user data on disk, now that WebSQL
    // has been deprecated and removed for all platforms except Android
    // WebView (crbug.com/333756088).
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](const base::FilePath& dir) { base::DeletePathRecursively(dir); },
            partition->GetPath().Append(storage::kDatabaseDirectoryName)));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  partition->GetBackgroundFetchContext()->Initialize();
}

}  // namespace content
