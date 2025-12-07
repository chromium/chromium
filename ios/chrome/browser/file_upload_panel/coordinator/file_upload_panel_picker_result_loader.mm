// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_picker_result_loader.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/bind_post_task.h"
#import "base/uuid.h"
#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_media_item.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"
#import "ios/web/public/web_state_id.h"

namespace {

std::optional<base::FilePath>
MoveLoadedFileRepresentationToTabChooseFileSubdirectory(
    web::WebStateID web_state_id,
    NSURL* file_url,
    NSError* error) {
  if (error) {
    return std::nullopt;
  }
  CHECK([file_url isFileURL]);
  const base::FilePath source_path = base::apple::NSURLToFilePath(file_url);
  std::optional<base::FilePath> temporary_subdirectory =
      CreateTabChooseFileSubdirectory(web_state_id);
  if (!temporary_subdirectory) {
    return std::nullopt;
  }
  const base::FilePath destination_path =
      temporary_subdirectory->Append(source_path.BaseName());
  if (base::Move(source_path, destination_path)) {
    return destination_path;
  }
  return std::nullopt;
}

}  // namespace

FileUploadPanelPickerResultLoader::FileUploadPanelPickerResultLoader(
    NSArray<PHPickerResult*>* results,
    web::WebStateID web_state_id)
    : results_(results), web_state_id_(web_state_id) {}

FileUploadPanelPickerResultLoader::~FileUploadPanelPickerResultLoader() =
    default;

void FileUploadPanelPickerResultLoader::Load(LoadResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loaded_items_ = [[NSMutableArray alloc] initWithCapacity:results_.count];
  LoadNextResult(0, std::move(callback));
}

void FileUploadPanelPickerResultLoader::LoadNextResult(
    NSUInteger index,
    LoadResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSUInteger count = results_.count;
  if (index >= count) {
    base::UmaHistogramCounts100(
        "IOS.FileUploadPanel.PhotoPicker.ResultLoader.FileCount",
        loaded_items_.count);
    base::UmaHistogramBoolean(
        "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", true);
    std::move(callback).Run(loaded_items_);
    return;
  }
  CHECK_LT(index, count);
  PHPickerResult* result = results_[index];
  LoadPickerResult(
      result, base::BindOnce(&FileUploadPanelPickerResultLoader::OnResultLoaded,
                             weak_ptr_factory_.GetWeakPtr(), index,
                             std::move(callback)));
}

void FileUploadPanelPickerResultLoader::OnResultLoaded(
    NSUInteger index,
    LoadResultCallback callback,
    FileUploadPanelMediaItem* loaded_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded_item) {
    base::UmaHistogramBoolean(
        "IOS.FileUploadPanel.PhotoPicker.ResultLoader.Result", false);
    std::move(callback).Run(nil);
    return;
  }
  [loaded_items_ addObject:loaded_item];
  LoadNextResult(index + 1, std::move(callback));
}

void FileUploadPanelPickerResultLoader::LoadPickerResult(
    PHPickerResult* result,
    base::OnceCallback<void(FileUploadPanelMediaItem*)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSArray<NSString*>* registered_type_identifiers =
      result.itemProvider.registeredTypeIdentifiers;
  UTType* file_type =
      FindTypeConformingToTarget(registered_type_identifiers, UTTypeMovie)
          ?: FindTypeConformingToTarget(registered_type_identifiers,
                                        UTTypeImage);
  if (!file_type) {
    std::move(callback).Run(nil);
    return;
  }
  auto completion_handler =
      base::BindOnce(MoveLoadedFileRepresentationToTabChooseFileSubdirectory,
                     web_state_id_)
          .Then(base::BindPostTaskToCurrentDefault(base::BindOnce(
              &FileUploadPanelPickerResultLoader::HandleMovedFileRepresentation,
              weak_ptr_factory_.GetWeakPtr(), file_type, std::move(callback))));
  [result.itemProvider
      loadFileRepresentationForTypeIdentifier:file_type.identifier
                            completionHandler:base::CallbackToBlock(std::move(
                                                  completion_handler))];
}

void FileUploadPanelPickerResultLoader::HandleMovedFileRepresentation(
    UTType* file_type,
    base::OnceCallback<void(FileUploadPanelMediaItem*)> callback,
    std::optional<base::FilePath> file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_path) {
    std::move(callback).Run(nil);
    return;
  }
  const BOOL is_video = [file_type conformsToType:UTTypeMovie];
  std::move(callback).Run([[FileUploadPanelMediaItem alloc]
      initWithFileURL:base::apple::FilePathToNSURL(*file_path)
              isVideo:is_video]);
}
