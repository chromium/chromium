// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_package.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_save_item_data.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_ukm_helper.h"
#include "components/download/public/common/download_utils.h"
#include "components/filename_generation/filename_generation.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/bad_message.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/save_file.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/download/save_item.h"
#include "content/browser/download/save_package_serialization_handler.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "content/public/common/referrer_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/request_mode.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace content {
namespace {

// Generates unique ids for SavePackage::unique_id_ field.
SavePackageId GetNextSavePackageId() {
  static SavePackageId::Generator g_save_package_id_generator;
  return g_save_package_id_generator.GenerateNextId();
}

// Default name which will be used when we can not get proper name from
// resource URL.
const char kDefaultSaveName[] = "saved_resource";

// Maximum number of file ordinal number. I think it's big enough for resolving
// name-conflict files which has same base file name.
const int32_t kMaxFileOrdinalNumber = 9999;

// Maximum length for file path. Since Windows have MAX_PATH limitation for
// file path, we need to make sure length of file path of every saved file
// is less than MAX_PATH
#if BUILDFLAG(IS_WIN)
const uint32_t kMaxFilePathLength = MAX_PATH - 1;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
const uint32_t kMaxFilePathLength = PATH_MAX - 1;
#endif

// Maximum length for file ordinal number part. Since we only support the
// maximum 9999 for ordinal number, which means maximum file ordinal number part
// should be "(9998)", so the value is 6.
const uint32_t kMaxFileOrdinalNumberPartLength = 6;

// Strip current ordinal number, if any. Should only be used on pure
// file names, i.e. those stripped of their extensions.
// TODO(estade): improve this to not choke on alternate encodings.
base::FilePath::StringType StripOrdinalNumber(
    const base::FilePath::StringType& base_name) {
  base::FilePath::StringType::size_type r_paren_index =
      base_name.rfind(FILE_PATH_LITERAL(')'));
  base::FilePath::StringType::size_type l_paren_index =
      base_name.rfind(FILE_PATH_LITERAL('('));
  if (l_paren_index >= r_paren_index)
    return base_name;

  for (base::FilePath::StringType::size_type i = l_paren_index + 1;
       i != r_paren_index; ++i) {
    if (!base::IsAsciiDigit(base_name[i]))
      return base_name;
  }

  return base_name.substr(0, l_paren_index);
}

// Check whether we can save page as complete-HTML for the contents which
// have specified a MIME type. Now only contents which have the MIME type
// "text/html" can be saved as complete-HTML.
bool CanSaveAsComplete(const std::string& contents_mime_type) {
  return contents_mime_type == "text/html" ||
         contents_mime_type == "application/xhtml+xml";
}

void CancelSavePackage(base::WeakPtr<SavePackage> save_package,
                       bool user_cancel) {
  if (save_package.get() && !save_package->canceled())
    save_package->Cancel(user_cancel, false);
}

const std::string GetMimeTypeForSaveType(SavePageType save_type) {
  switch (save_type) {
    case SAVE_PAGE_TYPE_AS_ONLY_HTML:
    case SAVE_PAGE_TYPE_AS_COMPLETE_HTML:
      return "text/html";
    case SAVE_PAGE_TYPE_AS_MHTML:
      return "multipart/related";
    case SAVE_PAGE_TYPE_UNKNOWN:
    case SAVE_PAGE_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
  NOTREACHED_IN_MIGRATION();
}

WebContents* GetWebContents(Page* page) {
  return WebContents::FromRenderFrameHost(&page->GetMainDocument());
}

const std::u16string& GetTitle(Page& page) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(&page.GetMainDocument());
  NavigationEntry* visible_entry =
      rfh->frame_tree()->controller().GetVisibleEntry();
  if (visible_entry)
    return visible_entry->GetTitleForDisplay();
  return base::EmptyString16();
}

bool IsSavableFrame(RenderFrameHost* rfh) {
  return rfh->IsRenderFrameLive() && !rfh->IsNestedWithinFencedFrame();
}

}  // namespace

const base::FilePath::CharType SavePackage::kDefaultHtmlExtension[] =
    FILE_PATH_LITERAL("html");

SavePackage::SavePackage(PageImpl& page)
    : page_(page.GetWeakPtrImpl()),
      page_url_(GetUrlToBeSaved(&page.GetMainDocument())),
      page_isolation_info_(
          page.GetMainDocument().ComputeIsolationInfoForNavigation(
              page_url_,
              page.GetMainDocument().IsCredentialless(),
              page.GetMainDocument()
                  .GetIsolationInfoForSubresources()
                  .nonce())),
      page_is_outermost_main_frame_(
          page.GetMainDocument().IsOutermostMainFrame()),
      title_(GetTitle(page)),
      start_tick_(base::TimeTicks::Now()),
      file_name_set_(&base::FilePath::CompareLessIgnoreCase),
      unique_id_(GetNextSavePackageId()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InternalInit();
}

// Used for tests.
SavePackage::SavePackage(PageImpl& page,
                         SavePageType save_type,
                         const base::FilePath& file_full_path,
                         const base::FilePath& directory_full_path)
    : page_(page.GetWeakPtrImpl()),
      page_url_(GetUrlToBeSaved(&page.GetMainDocument())),
      saved_main_file_path_(file_full_path),
      saved_main_directory_path_(directory_full_path),
      page_isolation_info_(
          page.GetMainDocument().ComputeIsolationInfoForNavigation(
              page_url_,
              page.GetMainDocument().IsCredentialless(),
              page.GetMainDocument()
                  .GetIsolationInfoForSubresources()
                  .nonce())),
      page_is_outermost_main_frame_(
          page.GetMainDocument().IsOutermostMainFrame()),
      title_(GetTitle(page)),
      start_tick_(base::TimeTicks::Now()),
      save_type_(save_type),
      file_name_set_(&base::FilePath::CompareLessIgnoreCase),
      unique_id_(GetNextSavePackageId()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK((save_type_ == SAVE_PAGE_TYPE_AS_ONLY_HTML) ||
         (save_type_ == SAVE_PAGE_TYPE_AS_MHTML) ||
         (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML))
      << save_type_;
  DCHECK(!saved_main_file_path_.empty() &&
         saved_main_file_path_.value().length() <= kMaxFilePathLength);
  DCHECK(!saved_main_directory_path_.empty() &&
         saved_main_directory_path_.value().length() < kMaxFilePathLength);
  InternalInit();
}

SavePackage::~SavePackage() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Stop receiving saving job's updates
  if (!finished_ && !canceled()) {
    // Unexpected quit.
    Cancel(true);
  }

  // We should no longer be observing the download::DownloadItem at this point.
  CHECK(!download_);

  DCHECK_EQ(all_save_items_count_,
            waiting_item_queue_.size() + completed_count() + in_process_count())
      << "waiting: " << waiting_item_queue_.size()
      << " completed: " << completed_count()
      << " in_progress: " << in_process_count();

  // Free all SaveItems.
  waiting_item_queue_.clear();
  in_progress_items_.clear();
  saved_success_items_.clear();
  saved_failed_items_.clear();
  // Clear containers that contain (now dangling/invalid) pointers to the
  // save items freed above.  This is not strictly required (as the containers
  // will be destructed soon by ~SavePackage), but seems like good code hygiene.
  frame_tree_node_id_to_contained_save_items_.clear();
  frame_tree_node_id_to_save_item_.clear();
  url_to_save_item_.clear();

  file_manager_ = nullptr;
}

void SavePackage::ClearPage() {
  page_ = nullptr;
}

// static
GURL SavePackage::GetUrlToBeSaved(RenderFrameHost* main_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_frame);
  // Retrieve the url from the NavigationEntry because it reflects its
  // origin rather than the displayed one (returned by GetVirtualURL) which may
  // be different (like having "view-source:" on the front).
  NavigationEntry* visible_entry =
      rfh->frame_tree()->controller().GetVisibleEntry();
  return visible_entry ? visible_entry->GetURL() : GURL();
}

void SavePackage::Cancel(bool user_action, bool cancel_download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!canceled()) {
    if (user_action)
      user_canceled_ = true;
    else
      disk_error_occurred_ = true;
    Stop(cancel_download_item);
  }
}

// Init() can be called directly, or indirectly via GetSaveInfo(). In both
// cases, we need file_manager_ to be initialized, so we do this first.
void SavePackage::InternalInit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(page_);

  file_manager_ = SaveFileManager::Get();
  DCHECK(file_manager_);

  download_manager_ = static_cast<DownloadManagerImpl*>(
      page_->GetMainDocument().GetBrowserContext()->GetDownloadManager());
  DCHECK(download_manager_);

  // Always constructed with the primary page that GetPageUkmSourceId()
  // supports.
  CHECK(page_->IsPrimary());
  ukm_source_id_ = page_->GetMainDocument().GetPageUkmSourceId();
  ukm_download_id_ = download::GetUniqueDownloadId();
  download::DownloadUkmHelper::RecordDownloadStarted(
      ukm_download_id_, ukm_source_id_, download::DownloadContent::TEXT,
      download::DownloadSource::UNKNOWN,
      download::CheckDownloadConnectionSecurity(
          page_->GetMainDocument().GetLastCommittedURL(),
          std::vector<GURL>{page_->GetMainDocument().GetLastCommittedURL()}),
      true /* is_same_host_download */);
}

bool SavePackage::Init(
    SavePackageDownloadCreatedCallback download_created_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(page_url_.is_valid());
  // Set proper running state.
  if (wait_state_ != INITIALIZE || !page_)
    return false;

  wait_state_ = START_PROCESS;

  // Initialize the request context and resource dispatcher.
  BrowserContext* browser_context =
      page_->GetMainDocument().GetBrowserContext();
  if (!browser_context) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  RenderFrameHost& frame_host = page_->GetMainDocument();
  download_manager_->CreateSavePackageDownloadItem(
      saved_main_file_path_, page_url_, GetMimeTypeForSaveType(save_type_),
      frame_host.GetProcess()->GetID(), frame_host.GetRoutingID(),
      base::BindOnce(&CancelSavePackage, weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SavePackage::InitWithDownloadItem,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(download_created_callback)));
  return true;
}

void SavePackage::InitWithDownloadItem(
    SavePackageDownloadCreatedCallback download_created_callback,
    download::DownloadItemImpl* item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(item);

  if (!page_)
    return;
  download_ = item;
  // Confirm above didn't delete the tab out from under us.
  if (!download_created_callback.is_null())
    std::move(download_created_callback).Run(download_.get());

  // Check save type and process the save page job.
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // Get directory
    DCHECK(!saved_main_directory_path_.empty());
    GetSavableResourceLinks();
  } else if (save_type_ == SAVE_PAGE_TYPE_AS_MHTML) {
    MHTMLGenerationParams mhtml_generation_params(saved_main_file_path_);
    GetWebContents(page_.get())
        ->GenerateMHTML(mhtml_generation_params,
                        base::BindOnce(&SavePackage::OnMHTMLGenerated, this));
  } else {
    DCHECK_EQ(SAVE_PAGE_TYPE_AS_ONLY_HTML, save_type_);
    wait_state_ = NET_FILES;
    // Add this item to waiting list.
    waiting_item_queue_.push_back(base::WrapUnique(new SaveItem(
        page_url_, Referrer(), page_isolation_info_,
        network::mojom::RequestMode::kNavigate, page_is_outermost_main_frame_,
        this, SaveFileCreateInfo::SAVE_FILE_FROM_NET, FrameTreeNodeId(),
        page_->GetMainDocument().GetFrameTreeNodeId())));
    all_save_items_count_ = 1;
    download_->SetTotalBytes(1);

    DoSavingProcess();
  }
}

void SavePackage::OnMHTMLGenerated(int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!download_)
    return;

  CHECK_EQ(download_->GetState(), download::DownloadItem::IN_PROGRESS);
  if (size <= 0) {
    Cancel(false);
    return;
  }
  wrote_to_completed_file_ = true;

  download_->OnAllDataSaved(size, std::unique_ptr<crypto::SecureHash>());

  auto* delegate = download_manager_->GetDelegate();
  if (!delegate || delegate->ShouldCompleteDownload(
                       download_, base::BindOnce(&SavePackage::Finish, this))) {
    Finish();
  }
}

// On POSIX, the length of |base_name| + |file_name_ext| is further
// restricted by NAME_MAX. The maximum allowed path looks like:
// '/path/to/save_dir' + '/' + NAME_MAX.
uint32_t SavePackage::GetMaxPathLengthForDirectory(
    const base::FilePath& base_dir) {
#if BUILDFLAG(IS_WIN)
  return kMaxFilePathLength;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return std::min(
      kMaxFilePathLength,
      static_cast<uint32_t>(base_dir.value().length()) + NAME_MAX + 1);
#endif
}

// static
bool SavePackage::TruncateBaseNameToFitPathConstraints(
    const base::FilePath& dir_path,
    const base::FilePath::StringType& file_name_ext,
    uint32_t max_file_path_len,
    base::FilePath::StringType* base_name) {
  DCHECK(!base_name->empty());
  int available_length = static_cast<int>(max_file_path_len -
                                          dir_path.value().length() -
                                          file_name_ext.length());
  // Need an extra space for the separator.
  if (!dir_path.EndsWithSeparator())
    --available_length;

  // Plenty of room.
  if (static_cast<int>(base_name->length()) <= available_length)
    return true;

  // Limited room. Truncate |base_name| to fit.
  if (available_length > 0) {
    *base_name = base_name->substr(0, available_length);
    return true;
  }

  // Not enough room to even use a shortened |base_name|.
  base_name->clear();
  return false;
}

// Generate name for saving resource.
bool SavePackage::GenerateFileName(const std::string& disposition,
                                   const GURL& url,
                                   bool need_html_ext,
                                   base::FilePath::StringType* generated_name) {
  // TODO(jungshik): Figure out the referrer charset when having one
  // makes sense and pass it to GenerateFileName.
  base::FilePath file_path = net::GenerateFileName(url,
                                                   disposition,
                                                   std::string(),
                                                   std::string(),
                                                   std::string(),
                                                   kDefaultSaveName);

  DCHECK(!file_path.empty());
  if (need_html_ext)
    file_path = file_path.ReplaceExtension(kDefaultHtmlExtension);

  DownloadManagerDelegate* delegate = download_manager_->GetDelegate();
  if (delegate)
    delegate->SanitizeSavePackageResourceName(&file_path, url);

  DCHECK_EQ(file_path.value(), file_path.BaseName().value())
      << "SanitizeSavePackageResourceName should only return a basename.";

  base::FilePath::StringType base_name =
      file_path.RemoveExtension().BaseName().value();
  base::FilePath::StringType file_name_ext = file_path.Extension();

  // Need to make sure the suggested file name is not too long.
  uint32_t max_path = GetMaxPathLengthForDirectory(saved_main_directory_path_);

  // Get safe pure file name.
  if (!TruncateBaseNameToFitPathConstraints(
          saved_main_directory_path_, file_name_ext, max_path, &base_name))
    return false;

  base::FilePath::StringType file_name = base_name + file_name_ext;

  // Check whether we already have same name in a case insensitive manner.
  auto iter = file_name_set_.find(file_name);
  if (iter == file_name_set_.end()) {
    DCHECK(!file_name.empty());
    file_name_set_.insert(file_name);
    generated_name->assign(file_name);
    return true;
  }

  // Found same name, increase the ordinal number for the file name.
  base_name = base::FilePath(*iter).RemoveExtension().BaseName().value();
  base::FilePath::StringType base_file_name = StripOrdinalNumber(base_name);

  // We need to make sure the length of base file name plus maximum ordinal
  // number path will be less than or equal to kMaxFilePathLength.
  if (!TruncateBaseNameToFitPathConstraints(
          saved_main_directory_path_, file_name_ext,
          max_path - kMaxFileOrdinalNumberPartLength, &base_file_name)) {
    return false;
  }

  // Prepare the new ordinal number.
  uint32_t ordinal_number;
  auto it = file_name_count_map_.find(base_file_name);
  if (it == file_name_count_map_.end()) {
    // First base-name-conflict resolving, use 1 as initial ordinal number.
    file_name_count_map_[base_file_name] = 1;
    ordinal_number = 1;
  } else {
    // We have met same base-name conflict, use latest ordinal number.
    ordinal_number = it->second;
  }

  if (ordinal_number > kMaxFileOrdinalNumber - 1) {
    // Use a random file from temporary file.
    base::FilePath temp_file;
    base::CreateTemporaryFile(&temp_file);
    file_name = temp_file.RemoveExtension().BaseName().value();
    // Get safe pure file name.
    if (!TruncateBaseNameToFitPathConstraints(saved_main_directory_path_,
                                              base::FilePath::StringType(),
                                              max_path, &file_name)) {
      return false;
    }
  } else {
    for (int i = ordinal_number; i < kMaxFileOrdinalNumber; ++i) {
      base::FilePath new_filepath(base_file_name);
      new_filepath = new_filepath
                         .InsertBeforeExtensionASCII(
                             base::StrCat({"(", base::NumberToString(i), ")"}))
                         .AddExtension(file_name_ext);
      base::FilePath::StringType new_name = new_filepath.value();
      if (!base::Contains(file_name_set_, new_name)) {
        // Resolved name conflict.
        file_name = new_name;
        file_name_count_map_[base_file_name] = ++i;
        break;
      }
    }
  }

  DCHECK(!file_name.empty());
  file_name_set_.insert(file_name);
  generated_name->assign(file_name);

  return true;
}

// We have received a message from SaveFileManager about a new saving job. We
// find a SaveItem and store it in our in_progress list.
void SavePackage::StartSave(const SaveFileCreateInfo* info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);

  if (!page_)
    return;

  auto it = in_progress_items_.find(info->save_item_id);
  if (it == in_progress_items_.end()) {
    // If not found, we must have cancel action.
    DCHECK(canceled());
    return;
  }
  SaveItem* save_item = it->second.get();

  DCHECK(!saved_main_file_path_.empty());

  // Determine the proper path for a saving job, by choosing either the default
  // save directory, or prompting the user.
  DCHECK(!save_item->has_final_name());
  if (info->url != page_url_) {
    base::FilePath::StringType generated_name;
    // For HTML resource file, make sure it will have .htm as extension name,
    // otherwise, when you open the saved page in Chrome again, download
    // file manager will treat it as downloadable resource, and download it
    // instead of opening it as HTML.
    bool need_html_ext =
        info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_DOM;
    if (!GenerateFileName(info->content_disposition,
                          GURL(info->url),
                          need_html_ext,
                          &generated_name)) {
      // We can not generate file name for this SaveItem, so we cancel the
      // saving page job if the save source is from serialized DOM data.
      // Otherwise, it means this SaveItem is sub-resource type, we treat it
      // as an error happened on saving. We can ignore this type error for
      // sub-resource links which will be resolved as absolute links instead
      // of local links in final saved contents.
      if (info->save_source == SaveFileCreateInfo::SAVE_FILE_FROM_DOM)
        Cancel(true);
      else
        SaveFinished(save_item->id(), 0, false);
      return;
    }

    // When saving page as only-HTML, we only have a SaveItem whose url
    // must be page_url_.
    DCHECK_EQ(SAVE_PAGE_TYPE_AS_COMPLETE_HTML, save_type_);
    DCHECK(!saved_main_directory_path_.empty());

    // Now we get final name retrieved from GenerateFileName, we will use it
    // rename the SaveItem.
    base::FilePath final_name =
        saved_main_directory_path_.Append(generated_name);
    save_item->SetTargetPath(final_name);
  } else {
    // It is the main HTML file, use the name chosen by the user.
    save_item->SetTargetPath(saved_main_file_path_);
  }

  // Check whether we begin to require serialized HTML data.
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML &&
      wait_state_ == HTML_DATA) {
    // Inform backend to serialize the all frames' DOM and send serialized
    // HTML data back.
    GetSerializedHtmlWithLocalLinks();
  }
}

SaveItem* SavePackage::LookupInProgressSaveItem(SaveItemId save_item_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = in_progress_items_.find(save_item_id);
  if (it != in_progress_items_.end()) {
    SaveItem* save_item = it->second.get();
    DCHECK_EQ(SaveItem::IN_PROGRESS, save_item->state());
    return save_item;
  }
  return nullptr;
}

void SavePackage::PutInProgressItemToSavedMap(SaveItem* save_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = in_progress_items_.find(save_item->id());
  CHECK(it != in_progress_items_.end(), base::NotFatalUntil::M130);
  DCHECK_EQ(save_item, it->second.get());
  std::unique_ptr<SaveItem> owned_item = std::move(it->second);
  in_progress_items_.erase(it);

  SaveItemIdMap& map = save_item->success() ?
      saved_success_items_ : saved_failed_items_;
  DCHECK(!base::Contains(map, save_item->id()));
  map[save_item->id()] = std::move(owned_item);
}

// Called for updating saving state.
bool SavePackage::UpdateSaveProgress(SaveItemId save_item_id,
                                     int64_t size,
                                     bool write_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem.
  SaveItem* save_item = LookupInProgressSaveItem(save_item_id);
  if (!save_item)
    return false;

  save_item->Update(size);

  // If we got disk error, cancel whole save page job.
  if (!write_success) {
    // Cancel job with reason of disk error.
    Cancel(false);
  }
  return true;
}

// Stop all page saving jobs that are in progress and instruct the download
// sequence to delete all saved files.
void SavePackage::Stop(bool cancel_download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If we haven't moved out of the initial state, there's nothing to cancel and
  // there won't be valid pointers for |file_manager_| or |download_|.
  if (wait_state_ == INITIALIZE)
    return;

  // When stopping, if it still has some items in in_progress, cancel them.
  DCHECK(canceled());
  if (in_process_count()) {
    for (const auto& it : in_progress_items_) {
      SaveItem* save_item = it.second.get();
      DCHECK_EQ(SaveItem::IN_PROGRESS, save_item->state());
      save_item->Cancel();
    }
    // Remove all in progress item to saved map. For failed items, they will
    // be put into saved_failed_items_, for successful item, they will be put
    // into saved_success_items_.
    while (in_process_count())
      PutInProgressItemToSavedMap(in_progress_items_.begin()->second.get());
  }

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its save_file_map_.
  std::vector<SaveItemId> save_item_ids;
  for (const auto& it : saved_success_items_)
    save_item_ids.push_back(it.first);
  for (const auto& it : saved_failed_items_)
    save_item_ids.push_back(it.first);

  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::RemoveSavedFileFromFileMap,
                                file_manager_, save_item_ids));

  finished_ = true;
  wait_state_ = FAILED;

  // Inform the download::DownloadItem we have canceled whole save page job.
  if (download_) {
    if (cancel_download_item)
      download_->Cancel(false);
    FinalizeDownloadEntry();
  }
}

void SavePackage::CheckFinish() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (in_process_count() || finished_ || !page_)
    return;

  DownloadManagerDelegate* delegate = download_manager_->GetDelegate();
  if (delegate) {
    std::vector<std::pair<SaveItemId, base::FilePath>> ids_and_final_paths(
        saved_success_items_.size());
    for (const auto& it : saved_success_items_)
      ids_and_final_paths.emplace_back(it.first, it.second->full_path());

    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveFileManager::GetSaveFilePaths, file_manager_,
                       std::move(ids_and_final_paths),
                       base::BindOnce(&SavePackage::CheckRenameAllowedForPaths,
                                      weak_ptr_factory_.GetWeakPtr())));
  } else {
    RenameIfAllowed(true);
  }
}

void SavePackage::CheckRenameAllowedForPaths(
    base::flat_map<base::FilePath, base::FilePath> tmp_paths_to_final_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DownloadManagerDelegate* delegate = download_manager_->GetDelegate();
  if (delegate) {
    delegate->CheckSavePackageAllowed(
        download_, std::move(tmp_paths_to_final_paths),
        base::BindOnce(&SavePackage::RenameIfAllowed,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    RenameIfAllowed(true);
  }
}

void SavePackage::RenameIfAllowed(bool allowed) {
  DCHECK(page_);
  if (!allowed) {
    Cancel(true);
    return;
  }

  base::FilePath dir = (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML &&
                        saved_success_items_.size() > 1) ?
                        saved_main_directory_path_ : base::FilePath();

  FinalNamesMap final_names;
  for (const auto& it : saved_success_items_)
    final_names.insert(std::make_pair(it.first, it.second->full_path()));

  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::RenameAllFiles, file_manager_,
                                final_names, dir,
                                page_->GetMainDocument().GetProcess()->GetID(),
                                page_->GetMainDocument().GetRoutingID(), id()));
}

// Successfully finished all items of this SavePackage.
void SavePackage::Finish() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // User may cancel the job when we're moving files to the final directory.
  if (canceled())
    return;

  wait_state_ = SUCCESSFUL;
  finished_ = true;

#if BUILDFLAG(IS_MAC)
  // Always set tags on the main HTML file, and if there is an associated
  // "_files" directory, set the tags on it, too.
  base::mac::SetFileTags(saved_main_file_path_, file_tags_);
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    base::mac::SetFileTags(saved_main_directory_path_, file_tags_);
  }
#endif  // BUILDFLAG(IS_MAC)

  if (download_) {
    std::vector<download::DownloadSaveItemData::ItemInfo> files;
    for (auto& item : saved_success_items_) {
      files.emplace_back(item.second->full_path(), item.second->url(),
                         item.second->referrer().url);
    }
    download::DownloadSaveItemData::AttachItemData(download_, std::move(files));
  }

  // TODO(qinmin): report the actual file size and duration for the download.
  download::DownloadUkmHelper::RecordDownloadCompleted(ukm_download_id_, 1,
                                                       base::TimeDelta(), 0);

  // This vector contains the save ids of the save files which SaveFileManager
  // needs to remove from its |save_file_map_|.
  std::vector<SaveItemId> list_of_failed_save_item_ids;
  for (const auto& it : saved_failed_items_) {
    const SaveItem* save_item = it.second.get();
    DCHECK_EQ(it.first, save_item->id());
    list_of_failed_save_item_ids.push_back(save_item->id());
  }

  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::RemoveSavedFileFromFileMap,
                                file_manager_, list_of_failed_save_item_ids));

  if (download_) {
    if (save_type_ != SAVE_PAGE_TYPE_AS_MHTML) {
      CHECK_EQ(download_->GetState(), download::DownloadItem::IN_PROGRESS);
      download_->DestinationUpdate(
          all_save_items_count_, CurrentSpeed(),
          std::vector<download::DownloadItem::ReceivedSlice>());
      download_->OnAllDataSaved(all_save_items_count_,
                                std::unique_ptr<crypto::SecureHash>());
    }
    download_->MarkAsComplete();

    if (download_->GetOpenWhenComplete())
      download_->OpenDownload();

    FinalizeDownloadEntry();
  }
}

void SavePackage::SaveFinished(SaveItemId save_item_id,
                               int64_t size,
                               bool is_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Because we might have canceled this saving job before,
  // so we might not find corresponding SaveItem. Just ignore it.
  SaveItem* save_item = LookupInProgressSaveItem(save_item_id);
  if (!save_item)
    return;

  // Let SaveItem set end state.
  save_item->Finish(size, is_success);
  // Remove the associated save id and SavePackage.
  file_manager_->RemoveSaveFile(save_item->id(), this);

  PutInProgressItemToSavedMap(save_item);

  // Inform the download::DownloadItem to update UI.
  // We use the received bytes as number of saved files.
  if (download_) {
    CHECK_EQ(download_->GetState(), download::DownloadItem::IN_PROGRESS);
    download_->DestinationUpdate(
        completed_count(), CurrentSpeed(),
        std::vector<download::DownloadItem::ReceivedSlice>());
  }

  if (save_item->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM &&
      save_item->url() == page_url_ && !save_item->received_bytes()) {
    // If size of main HTML page is 0, treat it as disk error.
    Cancel(false);
    return;
  }

  if (canceled()) {
    DCHECK(finished_);
    return;
  }

  // Continue processing the save page job.
  DoSavingProcess();

  // Check whether we can successfully finish whole job.
  CheckFinish();
}

void SavePackage::SaveCanceled(const SaveItem* save_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_manager_->RemoveSaveFile(save_item->id(), this);
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::CancelSave, file_manager_,
                                save_item->id()));
}

void SavePackage::SaveNextFile(bool process_all_remaining_items) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(page_);
  DCHECK(!waiting_item_queue_.empty());

  do {
    // Pop SaveItem from waiting list.
    std::unique_ptr<SaveItem> save_item =
        std::move(waiting_item_queue_.front());
    SaveItem* save_item_ptr = save_item.get();
    waiting_item_queue_.pop_front();

    // Add the item to |in_progress_items_|.
    DCHECK(!base::Contains(in_progress_items_, save_item->id()));
    in_progress_items_[save_item_ptr->id()] = std::move(save_item);
    save_item_ptr->Start();

    // Find the frame responsible for making the network request below - it will
    // be used in security checks made later.
    FrameTreeNodeId requester_frame_tree_node_id =
        save_item_ptr->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_NET
            ? save_item_ptr->container_frame_tree_node_id()
            : save_item_ptr->frame_tree_node_id();
    DCHECK(requester_frame_tree_node_id);
    FrameTreeNode* requester_frame_tree_node =
        FrameTreeNode::GloballyFindByID(requester_frame_tree_node_id);
    if (!requester_frame_tree_node) {
      save_item_ptr->Finish(0, /* is_success = */ false);
      continue;
    }
    RenderFrameHostImpl* requester_frame =
        requester_frame_tree_node->current_frame_host();

    mojo::PendingRemote<quarantine::mojom::Quarantine> quarantine;
    auto quarantine_callback =
        download_manager_->GetQuarantineConnectionCallback();
    if (quarantine_callback)
      quarantine_callback.Run(quarantine.InitWithNewPipeAndPassReceiver());

    file_manager_->SaveURL(
        save_item_ptr->id(), save_item_ptr->url(), save_item_ptr->referrer(),
        save_item_ptr->isolation_info(), save_item_ptr->request_mode(),
        save_item_ptr->is_outermost_main_frame(),
        requester_frame->GetProcess()->GetID(),
        requester_frame->render_view_host()->GetRoutingID(),
        requester_frame->GetRoutingID(), save_item_ptr->save_source(),
        save_item_ptr->full_path(),
        page_->GetMainDocument().GetBrowserContext(),
        page_->GetMainDocument()
            .GetRenderViewHost()
            ->GetProcess()
            ->GetStoragePartition(),
        this, download_manager_->GetApplicationClientIdForFileScanning(),
        std::move(quarantine));
  } while (process_all_remaining_items && !waiting_item_queue_.empty());
}

int SavePackage::PercentComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!all_save_items_count_)
    return 0;
  if (!in_process_count())
    return 100;
  return completed_count() / all_save_items_count_;
}

int64_t SavePackage::CurrentSpeed() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::TimeDelta diff = base::TimeTicks::Now() - start_tick_;
  int64_t diff_ms = diff.InMilliseconds();
  return diff_ms == 0 ? 0 : completed_count() * 1000 / diff_ms;
}

void SavePackage::DoSavingProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (save_type_ != SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // Save as HTML only or MHTML.
    DCHECK_EQ(NET_FILES, wait_state_);
    DCHECK((save_type_ == SAVE_PAGE_TYPE_AS_ONLY_HTML) ||
           (save_type_ == SAVE_PAGE_TYPE_AS_MHTML))
        << save_type_;
    if (waiting_item_queue_.size()) {
      DCHECK_EQ(all_save_items_count_, waiting_item_queue_.size());
      SaveNextFile(false);
    }
    return;
  }

  // We guarantee that images and JavaScripts must be downloaded first.
  // So when finishing all those sub-resources, we will know which
  // sub-resource's link can be replaced with local file path, which
  // sub-resource's link need to be replaced with absolute URL which
  // point to its internet address because it got error when saving its data.

  // Start a new SaveItem job if we still have job in waiting queue.
  if (!waiting_item_queue_.empty()) {
    DCHECK_EQ(NET_FILES, wait_state_);
    const SaveItem* save_item = waiting_item_queue_.front().get();
    if (save_item->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
      SaveNextFile(false);
    } else if (!in_process_count()) {
      // If there is no in-process SaveItem, it means all sub-resources
      // have been processed. Now we need to start serializing HTML DOM
      // for the current page to get the generated HTML data.
      wait_state_ = HTML_DATA;
      // All non-HTML resources have been finished, start all remaining
      // HTML files.
      SaveNextFile(true);
    }
  } else if (in_process_count()) {
    // Continue asking for HTML data.
    DCHECK_EQ(HTML_DATA, wait_state_);
  }
}

// After finishing all SaveItems which need to get data from net.
// We collect all URLs which have local storage and send the
// map:(originalURL:currentLocalPath) to render process (backend).
// Then render process will serialize DOM and send data to us.
void SavePackage::GetSerializedHtmlWithLocalLinks() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(page_);
  if (wait_state_ != HTML_DATA)
    return;

  // First collect the status of all the resource files and check whether they
  // have created local files (although they have not been completely saved).
  int successful_started_items_count = 0;
  for (const auto& item : in_progress_items_) {
    DCHECK_EQ(SaveFileCreateInfo::SAVE_FILE_FROM_DOM,
              item.second->save_source());
    if (item.second->has_final_name())
      successful_started_items_count++;
  }
  // If not all file of HTML resource have been started, then wait.
  if (successful_started_items_count != in_process_count())
    return;

  // Try to serialize all the frames gathered during GetSavableResourceLinks.
  DCHECK_EQ(0, number_of_frames_pending_response_);
  FrameTree* frame_tree =
      static_cast<RenderFrameHostImpl*>(&page_->GetMainDocument())
          ->frame_tree();
  for (const auto& item : frame_tree_node_id_to_save_item_) {
    FrameTreeNodeId frame_tree_node_id = item.first;
    const SaveItem* save_item = item.second;

    FrameTreeNode* frame_tree_node = frame_tree->FindByID(frame_tree_node_id);
    if (frame_tree_node &&
        IsSavableFrame(frame_tree_node->current_frame_host())) {
      // Ask the frame for HTML to be written to the associated SaveItem.
      GetSerializedHtmlWithLocalLinksForFrame(frame_tree_node);
      number_of_frames_pending_response_++;
    } else {
      // Notify SaveFileManager about the failure to save this SaveItem.
      download::GetDownloadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&SaveFileManager::SaveFinished, file_manager_,
                         save_item->id(), id(), false));
    }
  }
  if (number_of_frames_pending_response_ == 0) {
    // All frames disappeared since gathering of savable resources?
    // Treat this as cancellation.
    Cancel(false);
  }
}

void SavePackage::GetSerializedHtmlWithLocalLinksForFrame(
    FrameTreeNode* target_tree_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(target_tree_node);
  FrameTreeNodeId target_frame_tree_node_id =
      target_tree_node->frame_tree_node_id();
  RenderFrameHostImpl* target = target_tree_node->current_frame_host();

  // Collect all saved success items.
  // SECURITY NOTE: We don't send *all* urls / local paths, but only
  // those that the given frame had access to already (because it contained
  // the savable resources / subframes associated with save items).
  base::flat_map<GURL, base::FilePath> url_to_local_path;
  base::flat_map<blink::FrameToken, base::FilePath> frame_token_to_local_path;

  auto it = frame_tree_node_id_to_contained_save_items_.find(
      target_frame_tree_node_id);
  if (it != frame_tree_node_id_to_contained_save_items_.end()) {
    for (const SaveItem* save_item : it->second) {
      // Skip items that failed to save.
      if (!save_item->has_final_name()) {
        DCHECK_EQ(SaveItem::SaveState::COMPLETE, save_item->state());
        DCHECK(!save_item->success());
        continue;
      }

      // Calculate the relative path for referring to the |save_item|.
      base::FilePath local_path(base::FilePath::kCurrentDirectory);
      if (target_tree_node->IsOutermostMainFrame()) {
        local_path = local_path.Append(saved_main_directory_path_.BaseName());
      }
      local_path = local_path.Append(save_item->full_path().BaseName());

      // Insert the link into |url_to_local_path| or
      // |frame_token_to_local_path|.
      if (save_item->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) {
        DCHECK(!save_item->frame_tree_node_id());
        url_to_local_path[save_item->url()] = local_path;
      } else {
        FrameTreeNode* save_item_frame_tree_node =
            target_tree_node->frame_tree().FindByID(
                save_item->frame_tree_node_id());
        if (!save_item_frame_tree_node) {
          // crbug.com/541354: Raciness when saving a dynamically changing page.
          continue;
        }

        std::optional<blink::FrameToken> frame_token =
            save_item_frame_tree_node->render_manager()
                ->GetFrameTokenForSiteInstanceGroup(
                    target->GetSiteInstance()->group());

        DCHECK(frame_token.has_value());

        frame_token_to_local_path[frame_token.value()] = local_path;
      }
    }
  }

  // Create a SavePackageSerializationHandler for the target RenderFrameHost
  // plus the required callbacks to report progress, and make it owned by a mojo
  // receiver that will be alive for the time that the serialization process is
  // in progress. It's expected that the Done() callback will be called right
  // after the last time the DidReceiveData() callback gets invoked, at which
  // point the remote end of the pipe will be closed, disposing the receiver.
  mojo::PendingRemote<mojom::FrameHTMLSerializerHandler> serializer_handler;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SavePackageSerializationHandler>(
          base::BindRepeating(&SavePackage::OnDidReceiveSerializedHtmlData,
                              weak_ptr_factory_.GetWeakPtr(),
                              target->GetWeakPtr()),
          base::BindOnce(&SavePackage::OnDidFinishedSerializingHtmlData,
                         weak_ptr_factory_.GetWeakPtr(), target->GetWeakPtr())),
      serializer_handler.InitWithNewPipeAndPassReceiver());

  // Ask target frame to serialize itself.
  target->GetSerializedHtmlWithLocalLinks(
      url_to_local_path, frame_token_to_local_path,
      page_->GetMainDocument().GetBrowserContext()->IsOffTheRecord(),
      std::move(serializer_handler));
}

void SavePackage::OnDidReceiveSerializedHtmlData(
    base::WeakPtr<RenderFrameHostImpl> sender,
    const std::string& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Check current state.
  if (!sender || wait_state_ != HTML_DATA)
    return;

  // This method can only get called as a response to the serialization request
  // previously sent from the browser to the renderer for a given FrameTreeNode.
  const SaveItem* save_item = LookupSaveItemForSender(sender);
  DCHECK(save_item);

  if (save_item->state() != SaveItem::IN_PROGRESS) {
    for (const auto& saved_it : saved_success_items_) {
      if (saved_it.second->url() == save_item->url()) {
        wrote_to_completed_file_ = true;
        break;
      }
    }

    if (base::Contains(saved_failed_items_, save_item->id()))
      wrote_to_failed_file_ = true;

    return;
  }

  if (!data.empty()) {
    // Call write file functionality in download sequence.
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&SaveFileManager::UpdateSaveProgress,
                                  file_manager_, save_item->id(), data));
  }
}

void SavePackage::OnDidFinishedSerializingHtmlData(
    base::WeakPtr<RenderFrameHostImpl> sender) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Check current state.
  if (!sender || wait_state_ != HTML_DATA)
    return;

  // This method can only get called as a response to the serialization request
  // previously sent from the browser to the renderer for a given FrameTreeNode.
  const SaveItem* save_item = LookupSaveItemForSender(sender);
  DCHECK(save_item);

  // Current frame is completed saving, call finish in download sequence.
  DVLOG(20) << __func__ << "() save_item_id = " << save_item->id()
            << " url = \"" << save_item->url().spec() << "\"";
  download::GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveFileManager::SaveFinished, file_manager_,
                                save_item->id(), id(), true));
  number_of_frames_pending_response_--;
  DCHECK_LE(0, number_of_frames_pending_response_);
}

const SaveItem* SavePackage::LookupSaveItemForSender(
    base::WeakPtr<RenderFrameHostImpl> sender) {
  if (!sender)
    return nullptr;

  FrameTreeNodeId frame_tree_node_id =
      sender->frame_tree_node()->frame_tree_node_id();
  auto it = frame_tree_node_id_to_save_item_.find(frame_tree_node_id);
  if (it == frame_tree_node_id_to_save_item_.end())
    return nullptr;

  const SaveItem* save_item = it->second;
  DCHECK_EQ(SaveFileCreateInfo::SAVE_FILE_FROM_DOM, save_item->save_source());

  return save_item;
}

void SavePackage::GetSavableResourceLinksForRenderFrameHost(
    RenderFrameHostImpl* rfh) {
  if (!IsSavableFrame(rfh))
    return;
  ++number_of_frames_pending_response_;
  rfh->GetSavableResourceLinksFromRenderer();
}

// Ask for all savable resource links from backend, include main frame and
// sub-frame.
void SavePackage::GetSavableResourceLinks() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(page_);
  if (wait_state_ != START_PROCESS)
    return;

  wait_state_ = RESOURCES_LIST;

  DCHECK_EQ(0, number_of_frames_pending_response_);
  page_->GetMainDocument().ForEachRenderFrameHost(
      [this](RenderFrameHostImpl* rfh) {
        GetSavableResourceLinksForRenderFrameHost(rfh);
      });
  DCHECK_LT(0, number_of_frames_pending_response_);

  // Enqueue the main frame separately (because this frame won't show up in any
  // of GetsSavableResourceLinks callbacks).
  FrameTreeNode* main_frame_tree_node =
      static_cast<RenderFrameHostImpl*>(&page_->GetMainDocument())
          ->frame_tree_node();
  EnqueueFrame(FrameTreeNodeId(),  // No container.
               main_frame_tree_node->frame_tree_node_id(),
               main_frame_tree_node->current_url());
  all_save_items_count_ = 1;
}

void SavePackage::SavableResourceLinksResponse(
    RenderFrameHostImpl* sender,
    const std::vector<GURL>& resources_list,
    blink::mojom::ReferrerPtr referrer,
    const std::vector<blink::mojom::SavableSubframePtr>& subframes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (wait_state_ != RESOURCES_LIST)
    return;

  // Add all sub-resources to wait list.
  FrameTreeNodeId container_frame_tree_node_id =
      sender->frame_tree_node()->frame_tree_node_id();
  for (const GURL& u : resources_list) {
    EnqueueSavableResource(container_frame_tree_node_id, u,
                           referrer.To<content::Referrer>());
  }
  for (auto& subframe : subframes) {
    FrameTreeNode* subframe_ftn = sender->FindAndVerifyChild(
        subframe->subframe_token,
        bad_message::DWNLD_INVALID_SAVABLE_RESOURCE_LINKS_RESPONSE);

    if (!subframe_ftn) {
      // crbug.com/541354 - Raciness when saving a dynamically changing page.
      continue;
    }

    EnqueueFrame(container_frame_tree_node_id,
                 subframe_ftn->frame_tree_node_id(), subframe->original_url);
  }

  CompleteSavableResourceLinksResponse();
}

SaveItem* SavePackage::CreatePendingSaveItem(
    FrameTreeNodeId container_frame_tree_node_id,
    FrameTreeNodeId save_item_frame_tree_node_id,
    const GURL& url,
    const Referrer& referrer,
    SaveFileCreateInfo::SaveFileSource save_source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Referrer sanitized_referrer = Referrer::SanitizeForRequest(url, referrer);
  // Use an empty Isolation Info for subresources.
  // TODO(crbug.com/41485695): Populate the correct site isolation and fetch
  // mode per-resource.
  const net::IsolationInfo isolation_info;
  SaveItem* save_item = new SaveItem(
      url, sanitized_referrer, isolation_info,
      network::mojom::RequestMode::kNavigate, false, this, save_source,
      save_item_frame_tree_node_id, container_frame_tree_node_id);
  waiting_item_queue_.push_back(base::WrapUnique(save_item));

  frame_tree_node_id_to_contained_save_items_[container_frame_tree_node_id]
      .push_back(save_item);
  return save_item;
}

void SavePackage::CreatePendingSaveItemDeduplicatingByUrl(
    FrameTreeNodeId container_frame_tree_node_id,
    FrameTreeNodeId save_item_frame_tree_node_id,
    const GURL& url,
    const Referrer& referrer,
    SaveFileCreateInfo::SaveFileSource save_source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url.is_valid());  // |url| should be validated by the callers.

  // Frames should not be deduplicated by URL.
  DCHECK_NE(SaveFileCreateInfo::SAVE_FILE_FROM_DOM, save_source);

  auto it = url_to_save_item_.find(url);
  if (it != url_to_save_item_.end()) {
    frame_tree_node_id_to_contained_save_items_[container_frame_tree_node_id]
        .push_back(it->second);
  } else {
    url_to_save_item_[url] = CreatePendingSaveItem(container_frame_tree_node_id,
                                                   save_item_frame_tree_node_id,
                                                   url, referrer, save_source);
  }
}

void SavePackage::EnqueueSavableResource(
    FrameTreeNodeId container_frame_tree_node_id,
    const GURL& url,
    const Referrer& referrer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!url.is_valid())
    return;

  CreatePendingSaveItemDeduplicatingByUrl(
      container_frame_tree_node_id, FrameTreeNodeId(), url, referrer,
      SaveFileCreateInfo::SAVE_FILE_FROM_NET);
}

void SavePackage::EnqueueFrame(FrameTreeNodeId container_frame_tree_node_id,
                               FrameTreeNodeId frame_tree_node_id,
                               const GURL& frame_original_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SaveItem* save_item = CreatePendingSaveItem(
      container_frame_tree_node_id, frame_tree_node_id, frame_original_url,
      Referrer(), SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
  DCHECK(save_item);
  frame_tree_node_id_to_save_item_[frame_tree_node_id] = save_item;
}

void SavePackage::SavableResourceLinksError(RenderFrameHostImpl* sender) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CompleteSavableResourceLinksResponse();
}

void SavePackage::CompleteSavableResourceLinksResponse() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  --number_of_frames_pending_response_;
  DCHECK_LE(0, number_of_frames_pending_response_);
  if (number_of_frames_pending_response_ != 0)
    return;  // Need to wait for more responses from RenderFrames.

  // Sort |waiting_item_queue_| so that frames go last (frames are identified by
  // SAVE_FILE_FROM_DOM in the comparison function below).
  std::stable_sort(
      waiting_item_queue_.begin(), waiting_item_queue_.end(),
      [](const std::unique_ptr<SaveItem>& x,
         const std::unique_ptr<SaveItem>& y) {
        DCHECK(x);
        DCHECK(y);
        return (x->save_source() != SaveFileCreateInfo::SAVE_FILE_FROM_DOM) &&
               (y->save_source() == SaveFileCreateInfo::SAVE_FILE_FROM_DOM);
      });

  all_save_items_count_ = static_cast<int>(waiting_item_queue_.size());

  // We use total bytes as the total number of files we want to save.
  if (download_)
    download_->SetTotalBytes(all_save_items_count_);

  if (all_save_items_count_) {
    wait_state_ = NET_FILES;

    // Give backend the lists which contain all resource links that have local
    // storage, after which, render process will serialize DOM for generating
    // HTML data.
    DoSavingProcess();
  } else {
    // No savable frames and/or resources - treat it as user cancel.
    Cancel(true);
  }
}

void SavePackage::GetSaveInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!page_)
    return;
  // Can't use |page_| in the download sequence, so get the data that we
  // need before calling to it.
  base::FilePath website_save_dir;
  base::FilePath download_save_dir;
  auto* delegate = download_manager_->GetDelegate();
  if (delegate) {
    delegate->GetSaveDir(page_->GetMainDocument().GetBrowserContext(),
                         &website_save_dir, &download_save_dir);
  }
  std::string mime_type =
      static_cast<PageImpl*>(page_.get())->GetContentsMimeType();
  bool can_save_as_complete = CanSaveAsComplete(mime_type);
  download::GetDownloadTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SavePackage::CreateDirectoryOnFileThread, title_,
                     page_url_, can_save_as_complete, mime_type,
                     website_save_dir, download_save_dir),
      base::BindOnce(&SavePackage::ContinueGetSaveInfo, this,
                     can_save_as_complete));
}

// static
base::FilePath SavePackage::CreateDirectoryOnFileThread(
    const std::u16string& title,
    const GURL& page_url,
    bool can_save_as_complete,
    const std::string& mime_type,
    const base::FilePath& website_save_dir,
    const base::FilePath& download_save_dir) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath suggested_filename = filename_generation::GenerateFilename(
      title, page_url, can_save_as_complete, mime_type);

  base::FilePath save_dir;
  // If the default html/websites save folder doesn't exist...
  if (!base::DirectoryExists(website_save_dir)) {
    // If the default download dir doesn't exist, create it.
    if (!base::DirectoryExists(download_save_dir)) {
      bool res = base::CreateDirectory(download_save_dir);
      DCHECK(res);
    }
    save_dir = download_save_dir;
  } else {
    // If it does exist, use the default save dir param.
    save_dir = website_save_dir;
  }

  base::FilePath::StringType base_name =
      suggested_filename.RemoveExtension().BaseName().value();
  base::FilePath::StringType file_name_ext = suggested_filename.Extension();

  // Need to make sure the suggested file name is not too long.
  uint32_t max_path = GetMaxPathLengthForDirectory(save_dir);

  if (TruncateBaseNameToFitPathConstraints(save_dir, file_name_ext, max_path,
                                           &base_name)) {
    return save_dir.Append(base_name + file_name_ext);
  }
  // Cannot create a shorter filename. This will cause the save as operation
  // to fail unless the user pick a shorter name. Continuing even though it
  // will fail because returning means no save as popup for the user, which
  // is even more confusing. This case should be rare though.
  return save_dir.Append(suggested_filename);
}

void SavePackage::ContinueGetSaveInfo(bool can_save_as_complete,
                                      const base::FilePath& suggested_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The WebContents which owns this SavePackage may have disappeared during
  // the UI->download sequence->UI thread hop of
  // GetSaveInfo->CreateDirectoryOnFileThread->ContinueGetSaveInfo.
  if (!page_ || !download_manager_->GetDelegate())
    return;

  base::FilePath::StringType default_extension;
  if (can_save_as_complete)
    default_extension = kDefaultHtmlExtension;

  download_manager_->GetDelegate()->ChooseSavePath(
      GetWebContents(page_.get()), suggested_path, default_extension,
      can_save_as_complete,
      base::BindOnce(&SavePackage::OnPathPicked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavePackage::OnPathPicked(
    SavePackagePathPickedParams params,
    SavePackageDownloadCreatedCallback download_created_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK((params.save_type == SAVE_PAGE_TYPE_AS_ONLY_HTML) ||
         (params.save_type == SAVE_PAGE_TYPE_AS_MHTML) ||
         (params.save_type == SAVE_PAGE_TYPE_AS_COMPLETE_HTML))
      << params.save_type;
  if (!page_)
    return;
  // Ensure the filename is safe.
  saved_main_file_path_ = params.file_path;
  // TODO(asanka): This call may block on IO and shouldn't be made
  // from the UI thread.  See http://crbug.com/61827.
  std::string mime_type =
      static_cast<PageImpl*>(page_.get())->GetContentsMimeType();
  net::GenerateSafeFileName(mime_type, false, &saved_main_file_path_);

  saved_main_directory_path_ = saved_main_file_path_.DirName();
  save_type_ = params.save_type;
  if (save_type_ == SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // Make new directory for saving complete file.
    saved_main_directory_path_ = saved_main_directory_path_.Append(
        saved_main_file_path_.RemoveExtension().BaseName().value() +
        FILE_PATH_LITERAL("_files"));
  }

#if BUILDFLAG(IS_MAC)
  file_tags_ = params.file_tags;
#endif

  Init(std::move(download_created_callback));
}

void SavePackage::FinalizeDownloadEntry() {
  DCHECK(download_);
  DCHECK(download_manager_);
  download_ = nullptr;
  download_manager_ = nullptr;
}

}  // namespace content
