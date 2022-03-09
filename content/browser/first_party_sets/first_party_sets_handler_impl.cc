// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kPersistedFirstPartySetsFileName[] =
    FILE_PATH_LITERAL("persisted_first_party_sets.json");

// Reads the sets as raw JSON from their storage file, returning the raw sets on
// success and empty string on failure.
std::string LoadSetsFromDisk(const base::FilePath& path) {
  DCHECK(!path.empty());

  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    VLOG(1) << "Failed loading serialized First-Party Sets file from "
            << path.MaybeAsASCII();
    return "";
  }
  return result;
}

// Writes the sets as raw JSON to the storage file.
//
// TODO(crbug.com/1219656): To handle the cases of file corrupting due to
// incomplete writes, write to a temp file then rename over the old file.
void MaybeWriteSetsToDisk(const base::FilePath& path, const std::string& sets) {
  DCHECK(!path.empty());

  if (!base::WriteFile(path, sets)) {
    VLOG(1) << "Failed writing serialized First-Party Sets to file "
            << path.MaybeAsASCII();
  }
}

}  // namespace

// static
FirstPartySetsHandler* FirstPartySetsHandler::GetInstance() {
  return FirstPartySetsHandlerImpl::GetInstance();
}

// static
FirstPartySetsHandlerImpl* FirstPartySetsHandlerImpl::GetInstance() {
  static base::NoDestructor<FirstPartySetsHandlerImpl> instance;
  return instance.get();
}

FirstPartySetsHandlerImpl::FirstPartySetsHandlerImpl() = default;
FirstPartySetsHandlerImpl::~FirstPartySetsHandlerImpl() = default;

void FirstPartySetsHandlerImpl::SendAndUpdatePersistedSets(
    const base::FilePath& user_data_dir,
    base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                            const std::string&)> send_sets) {
  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed loading serialized First-Party Sets file.";
    SendPersistedSets(std::move(send_sets), base::FilePath(), "");
    return;
  }

  const base::FilePath persisted_sets_path =
      user_data_dir.Append(kPersistedFirstPartySetsFileName);

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  // base::Unretained(this) is safe here because this is a static singleton.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&LoadSetsFromDisk, persisted_sets_path),
      base::BindOnce(&FirstPartySetsHandlerImpl::SendPersistedSets,
                     base::Unretained(this), std::move(send_sets),
                     persisted_sets_path));
}

void FirstPartySetsHandlerImpl::OnGetUpdatedSets(const base::FilePath& path,
                                                 const std::string& sets) {
  if (!path.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&MaybeWriteSetsToDisk, path, sets));
  }
}

void FirstPartySetsHandlerImpl::SendPersistedSets(
    base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                            const std::string&)> send_sets,
    const base::FilePath& path,
    const std::string& sets) {
  // base::Unretained(this) is safe here because this is a static singleton.
  std::move(send_sets).Run(
      base::BindOnce(&FirstPartySetsHandlerImpl::OnGetUpdatedSets,
                     base::Unretained(this), path),
      sets);
}

void FirstPartySetsHandlerImpl::SetPublicFirstPartySets(base::File sets_file) {
  content::GetNetworkService()->SetFirstPartySets(std::move(sets_file));
}

}  // namespace content
