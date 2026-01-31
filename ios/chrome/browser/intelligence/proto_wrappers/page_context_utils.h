// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_UTILS_H_

#import <optional>
#import <string>

#import "components/autofill/core/common/unique_ids.h"

// TODO(crbug.com/458081684): Move away from all autofill dependencies once
// the migration in ios/web is done for frame registration.

namespace web {
class WebState;
}  // namespace web

// Returns true if the context for `web_state` can be extracted. PageContexts
// are available for HTML and image pages that use http/https schemes. Namely,
// this filters out PDFs, NTPs and chrome:// pages.
bool CanExtractPageContextForWebState(web::WebState* web_state);

// Deserializes a string frame ID into a LocalFrameToken.
std::optional<autofill::LocalFrameToken> DeserializeFrameIdAsLocalFrameToken(
    const std::string& serialized_id);

// Deserializes a string frame ID into a RemoteFrameToken.
std::optional<autofill::RemoteFrameToken> DeserializeFrameIdAsRemoteFrameToken(
    const std::string& serialized_id);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_UTILS_H_
