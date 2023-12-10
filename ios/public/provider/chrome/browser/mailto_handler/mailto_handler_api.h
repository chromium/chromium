// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_API_H_

#import <memory>

#import "ios/chrome/browser/mailto_handler/model/mailto_handler_configuration.h"

class MailtoHandlerService;

namespace ios {
namespace provider {

// Creates a new instance of MailtoHandlerService.
std::unique_ptr<MailtoHandlerService> CreateMailtoHandlerService(
    MailtoHandlerConfiguration* configuration);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_API_H_
