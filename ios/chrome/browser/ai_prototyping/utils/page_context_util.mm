// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/scoped_observation.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace {
// Limit the lengh of sanitized url used as filename to prevent error from
// filename being too long.
NSUInteger kUrlLengthLimit = 50;

// Self-deleting observer that waits for a page to finish loading.
class SelfDestructivePageLoadObserver : public web::WebStateObserver {
 public:
  SelfDestructivePageLoadObserver(web::WebState* web_state,
                                  base::OnceClosure callback)
      : callback_(std::move(callback)) {
    observation_.Observe(web_state);
  }

 private:
  ~SelfDestructivePageLoadObserver() override = default;

  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override {
    std::move(callback_).Run();
    observation_.Reset();
    delete this;
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    observation_.Reset();
    delete this;
  }

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
  base::OnceClosure callback_;
};

SavePageContextResult SaveProtoToPath(
    const optimization_guide::proto::PageContext& page_context,
    const base::FilePath& file_path) {
  SavePageContextResult result;

  // Create the output directory if necessary.
  NSString* dir = base::SysUTF8ToNSString(file_path.DirName().value());
  if (![[NSFileManager defaultManager] createDirectoryAtPath:dir
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:nil]) {
    result.error_message =
        base::StringPrintf("Could not create output directory %s",
                           file_path.DirName().value().c_str());
  }

  // Convert base::FilePath path to a C_style string for fopen.
  const char* c_file_path = file_path.value().c_str();
  if (c_file_path == nullptr) {
    result.error_message = "Could not convert file path to C_style string.";
    return result;
  }

  // Open the file for writing in binary mode and get the file descriptor.
  FILE* fp = fopen(c_file_path, "wb");
  if (fp == nullptr) {
    result.error_message =
        base::StringPrintf("Could not open file %s for writing. Error: %s",
                           c_file_path, strerror(errno));
    return result;
  }
  // Get the file descriptor from the FILE pointer.
  int fd = fileno(fp);
  if (fd == -1) {
    result.error_message =
        base::StringPrintf("Could not get file descriptor for %s. Error: %s",
                           c_file_path, strerror(errno));
    fclose(fp);
    return result;
  }

  // Serialize and write the message to the file.
  bool success = page_context.SerializeToFileDescriptor(fd);
  // Close the file
  if (fclose(fp) != 0) {
    result.error_message =
        base::StringPrintf("Could not close file '%s' properly. Error: %s",
                           c_file_path, strerror(errno));
    return result;
  }
  if (!success) {
    result.error_message = base::StringPrintf(
        "Failed to serialize protobuf message to file: '%s'. Error: %s",
        c_file_path, strerror(errno));
    return result;
  }
  result.success = true;
  result.file_path = file_path;
  return result;
}

base::FilePath GetDirectoryPath() {
  // Get the Documents directory path.
  return base::apple::NSStringToFilePath([NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSAllDomainsMask, YES) firstObject]);
}
}  // namespace

SavePageContextResult::SavePageContextResult() = default;
SavePageContextResult::~SavePageContextResult() = default;
SavePageContextResult::SavePageContextResult(SavePageContextResult&&) = default;
SavePageContextResult& SavePageContextResult::operator=(
    SavePageContextResult&&) = default;

PageContextWrapper* CreatePageContextWrapper(
    web::WebState* web_state,
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        completion_callback) {
  PageContextWrapper* page_context_wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:std::move(completion_callback)];
  [page_context_wrapper setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper setShouldGetSnapshot:YES];
  [page_context_wrapper setShouldGetFullPagePDF:YES];
  return page_context_wrapper;
}

void PopulatePageContext(PageContextWrapper* wrapper,
                         web::WebState* web_state) {
  if (web_state->IsLoading()) {
    __weak PageContextWrapper* weak_wrapper = wrapper;
    new SelfDestructivePageLoadObserver(
        web_state, base::BindOnce(^{
          if (weak_wrapper) {
            [weak_wrapper populatePageContextFieldsAsync];
          }
        }));
  } else {
    [wrapper populatePageContextFieldsAsync];
  }
}

void PopulatePageContextWithTimeout(PageContextWrapper* wrapper,
                                    web::WebState* web_state,
                                    base::TimeDelta timeout) {
  if (web_state->IsLoading()) {
    __weak PageContextWrapper* weak_wrapper = wrapper;
    new SelfDestructivePageLoadObserver(
        web_state, base::BindOnce(^{
          if (weak_wrapper) {
            [weak_wrapper populatePageContextFieldsAsyncWithTimeout:timeout];
          }
        }));
  } else {
    [wrapper populatePageContextFieldsAsyncWithTimeout:timeout];
  }
}

SavePageContextResult SaveSerializedPageContextToDisk(
    const optimization_guide::proto::PageContext& page_context,
    const std::string& dir_name,
    const std::string& file_name) {
  base::FilePath directory_path = GetDirectoryPath();
  base::FilePath file_path = directory_path.Append(dir_name).Append(file_name);
  return SaveProtoToPath(page_context, file_path);
}

SavePageContextResult SaveSerializedPageContextToDisk(
    const optimization_guide::proto::PageContext& page_context) {
  base::FilePath directory_path = GetDirectoryPath();
  std::string file_name = FileNameForPageContext(page_context);
  base::FilePath file_path = directory_path.Append(file_name);
  return SaveProtoToPath(page_context, file_path);
}

std::string FileNameForPageContext(
    const optimization_guide::proto::PageContext& page_context) {
  NSString* urlString = base::SysUTF8ToNSString(page_context.url());
  if ([urlString length] > kUrlLengthLimit) {
    urlString = [urlString substringToIndex:kUrlLengthLimit];
  }
  NSString* fileName =
      [SanitizeUrl(urlString) stringByAppendingString:@".txtpb"];
  return base::SysNSStringToUTF8(fileName);
}

NSString* SanitizeUrl(NSString* url) {
  NSCharacterSet* illegalFileNameCharacters =
      [NSCharacterSet characterSetWithCharactersInString:@"/\\?%*|\"<>:"];
  return [[url componentsSeparatedByCharactersInSet:illegalFileNameCharacters]
      componentsJoinedByString:@""];
}
