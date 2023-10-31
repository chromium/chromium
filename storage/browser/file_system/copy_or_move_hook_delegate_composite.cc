// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/copy_or_move_hook_delegate_composite.h"

#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
namespace {

void StatusReduction(CopyOrMoveHookDelegate::StatusCallback callback,
                     std::vector<std::pair<int, base::File::Error>> errors) {
  std::sort(errors.begin(), errors.end());
  for (std::pair<int, base::File::Error> status : errors) {
    if (status.second != base::File::FILE_OK) {
      std::move(callback).Run(status.second);
      return;
    }
  }
  std::move(callback).Run(base::File::FILE_OK);
}

void ErrorReduction(CopyOrMoveHookDelegate::ErrorCallback callback,
                    std::vector<CopyOrMoveHookDelegate::ErrorAction> actions) {
  for (CopyOrMoveHookDelegate::ErrorAction action : actions) {
    if (action != CopyOrMoveHookDelegate::ErrorAction::kDefault) {
      std::move(callback).Run(action);
      return;
    }
  }
  std::move(callback).Run(CopyOrMoveHookDelegate::ErrorAction::kDefault);
}

base::RepeatingCallback<void(std::pair<int, base::File::Error>)> CreateBarrier(
    size_t size,
    CopyOrMoveHookDelegate::StatusCallback callback) {
  return base::BarrierCallback<std::pair<int, base::File::Error>>(
      size, base::BindOnce(&StatusReduction, std::move(callback)));
}

base::RepeatingCallback<void(CopyOrMoveHookDelegate::ErrorAction)>
CreateErrorBarrier(size_t size,
                   CopyOrMoveHookDelegate::ErrorCallback callback) {
  return base::BarrierCallback<CopyOrMoveHookDelegate::ErrorAction>(
      size, base::BindOnce(&ErrorReduction, std::move(callback)));
}

CopyOrMoveHookDelegate::StatusCallback CreateStatusCallback(
    base::RepeatingCallback<void(std::pair<int, base::File::Error>)> barrier,
    size_t index) {
  return base::BindOnce(
      [](base::RepeatingCallback<void(std::pair<int, base::File::Error>)>
             barrier,
         size_t index, base::File::Error status) {
        std::move(barrier).Run(std::make_pair(index, status));
      },
      barrier, index);
}

CopyOrMoveHookDelegate::ErrorCallback CreateErrorCallback(
    base::RepeatingCallback<void(CopyOrMoveHookDelegate::ErrorAction)>
        barrier) {
  return base::BindOnce(
      [](base::RepeatingCallback<void(CopyOrMoveHookDelegate::ErrorAction)>
             barrier,
         CopyOrMoveHookDelegate::ErrorAction action) {
        std::move(barrier).Run(action);
      },
      barrier);
}
}  // namespace

// static
std::unique_ptr<CopyOrMoveHookDelegate>
CopyOrMoveHookDelegateComposite::CreateOrAdd(
    std::unique_ptr<CopyOrMoveHookDelegate> parent,
    std::unique_ptr<CopyOrMoveHookDelegate> child) {
  DCHECK(parent != nullptr) << "parent must not be null";
  DCHECK(child != nullptr) << "child must not be null";
  auto composite =
      std::make_unique<CopyOrMoveHookDelegateComposite>(std::move(parent));
  composite->Add(std::move(child));
  return std::move(composite);
}

CopyOrMoveHookDelegateComposite::CopyOrMoveHookDelegateComposite()
    : CopyOrMoveHookDelegate(/*is_composite=*/true) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CopyOrMoveHookDelegateComposite::~CopyOrMoveHookDelegateComposite() = default;

void CopyOrMoveHookDelegateComposite::OnBeginProcessFile(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto barrier = CreateBarrier(delegates_.size(), std::move(callback));
  for (size_t i = 0; i < delegates_.size(); ++i) {
    delegates_[i]->OnBeginProcessFile(source_url, destination_url,
                                      CreateStatusCallback(barrier, i));
  }
}

void CopyOrMoveHookDelegateComposite::OnBeginProcessDirectory(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto barrier = CreateBarrier(delegates_.size(), std::move(callback));
  for (size_t i = 0; i < delegates_.size(); ++i) {
    delegates_[i]->OnBeginProcessDirectory(source_url, destination_url,
                                           CreateStatusCallback(barrier, i));
  }
}

void CopyOrMoveHookDelegateComposite::OnProgress(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (std::unique_ptr<CopyOrMoveHookDelegate>& delegate : delegates_) {
    delegate->OnProgress(source_url, destination_url, size);
  }
}

void CopyOrMoveHookDelegateComposite::OnError(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error,
    ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto barrier = CreateErrorBarrier(delegates_.size(), std::move(callback));
  for (std::unique_ptr<CopyOrMoveHookDelegate>& delegate : delegates_) {
    delegate->OnError(source_url, destination_url, error,
                      CreateErrorCallback(barrier));
  }
}

void CopyOrMoveHookDelegateComposite::OnEndCopy(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (std::unique_ptr<CopyOrMoveHookDelegate>& delegate : delegates_) {
    delegate->OnEndCopy(source_url, destination_url);
  }
}

void CopyOrMoveHookDelegateComposite::OnEndMove(
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (std::unique_ptr<CopyOrMoveHookDelegate>& delegate : delegates_) {
    delegate->OnEndMove(source_url, destination_url);
  }
}

void CopyOrMoveHookDelegateComposite::OnEndRemoveSource(
    const FileSystemURL& source_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (std::unique_ptr<CopyOrMoveHookDelegate>& delegate : delegates_) {
    delegate->OnEndRemoveSource(source_url);
  }
}

CopyOrMoveHookDelegateComposite::CopyOrMoveHookDelegateComposite(
    std::unique_ptr<CopyOrMoveHookDelegate> delegate)
    : CopyOrMoveHookDelegate(/*is_composite=*/true) {
  if (delegate->is_composite_) {
    CopyOrMoveHookDelegateComposite* composite =
        static_cast<CopyOrMoveHookDelegateComposite*>(delegate.get());
    delegates_ = std::move(composite->delegates_);
  } else {
    delegates_.push_back(std::move(delegate));
  }
}

void CopyOrMoveHookDelegateComposite::Add(
    std::unique_ptr<CopyOrMoveHookDelegate> delegate) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  delegates_.push_back(std::move(delegate));
}

}  // namespace storage
