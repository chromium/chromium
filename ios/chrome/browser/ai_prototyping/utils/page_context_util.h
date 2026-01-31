// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_CONTEXT_UTIL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_CONTEXT_UTIL_H_

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

namespace base {
class FilePath;
class TimeDelta;
}
@class PageContextWrapper;
namespace web {
class WebState;
}
namespace optimization_guide::proto {
class PageContext;
}

// The result of saving a page context to disk.
struct SavePageContextResult {
  // True if the page context was saved successfully.
  bool success = false;
  // The path to the saved file.
  base::FilePath file_path;
  // An error message if `success` is false.
  std::string error_message;

  SavePageContextResult();
  ~SavePageContextResult();
  SavePageContextResult(SavePageContextResult&&);
  SavePageContextResult& operator=(SavePageContextResult&&);
};

PageContextWrapper* CreatePageContextWrapper(
    web::WebState* web_state,
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        completion_callback);

// Populates the page context from the `wrapper` for the given `web_state`.
// If the page is still loading, it waits for it to finish before populating.
void PopulatePageContext(PageContextWrapper* wrapper, web::WebState* web_state);

// Same as `PopulatePageContext` but with a `timeout`.
void PopulatePageContextWithTimeout(PageContextWrapper* wrapper,
                                    web::WebState* web_state,
                                    base::TimeDelta timeout);

// Serializes the given `page_context` and saves it to a file on disk.
SavePageContextResult SaveSerializedPageContextToDisk(
    const optimization_guide::proto::PageContext& page_context);

// Serializes the given `page_context` and saves it to a file on disk with the
// given `dir_name` and `file_name`.
SavePageContextResult SaveSerializedPageContextToDisk(
    const optimization_guide::proto::PageContext& page_context,
    const std::string& dir_name,
    const std::string& file_name);

std::string FileNameForPageContext(
    const optimization_guide::proto::PageContext& page_context);

// Sanitze give `url` to be used as file name.
NSString* SanitizeUrl(NSString* url);

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_CONTEXT_UTIL_H_
