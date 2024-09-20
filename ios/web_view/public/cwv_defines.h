// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_DEFINES_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_DEFINES_H_

// These defines are used to indicate support for a particular API. This can be
// useful in cases where a client depends on more than one version of this
// library, depending on the clients own configuration.

// Supports -[CWVWebViewConfiguration nonPersistentConfiguration].
#define IOS_WEB_VIEW_SUPPORTS_CWV_CONFIGURATION_NON_PERSISTENT_CONFIGURATION 1

// Supports -[CWVNavigationDelegate didStartNavigation:] and deprecates
// -[CWVNavigationDelegate didStartProvisionalNavigation:].
#define IOS_WEB_VIEW_SUPPORTS_CWV_NAVIGATION_DELEGATE_DID_START_NAVIGATION 1

// -[CWVPassword isBlocked] instead of -[CWVPassword isBlacklisted].
#define IOS_WEB_VIEW_SUPPORTS_CWV_PASSWORD_BLOCKED 1

// Implements -[CWVWebView becomeFirstResponder] to support keyboard commands.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_BECOME_FIRST_RESPONDER 1

// Indicates that CWVWebView already has a custom UIDropInteraction installed.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_CUSTOM_DROP_INTERACTION 1

// Allows customization of the keyboard input accessory view for CWVWebView.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_INPUT_ACCESSORY_VIEW 1

// Allows customization of CWVWebView's user agent string.
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEB_VIEW_CUSTOM_USER_AGENT 1

// Allows handling of SSL errors with CWVSSLErrorHandler.
#define IOS_WEB_VIEW_SUPPORTS_CWV_SSL_ERROR_HANDLER 1

// Allows new save API for CWVCreditCardSaver and deletes the previous fixer
// based flows.
#define IOS_WEB_VIEW_SUPPORTS_NEW_CREDIT_CARD_SAVE_APIS 1

// Supports APIs used to implement the iOS credential provider extension.
#define IOS_WEB_VIEW_SUPPORTS_CREDENTIAL_EXTENSION_PROVIDER_APIS 1

// Supports new APIs that simplifies how CWVSyncController is used.
#define IOS_WEB_VIEW_SUPPORTS_NEW_CWV_SYNC_CONTROLLER_APIS 1

// Supports APIs used to implement the trusted vault for chrome sync.
#define IOS_WEB_VIEW_SUPPORTS_TRUSTED_VAULT_APIS 1

// Supports -[CWVAutofillDataManager updatePassword:newUsername:newPassword:].
#define IOS_WEB_VIEW_SUPPORTS_UPDATING_PASSWORDS 1

// Supports -[CWVAutofillDataManager addPasswordWithUsername:password:site:].
#define IOS_WEB_VIEW_SUPPORTS_ADDING_PASSWORDS 1

// Supports -[CWVNavigationDelegate handleLookalikeURLWithHandler:].
#define IOS_WEB_VIEW_SUPPORTS_CWV_LOOKALIKE_URL_HANDLER 1

// Allows accessing SSL certificate details through the CWVWebView.
#define IOS_WEB_VIEW_SUPPORTS_CWV_X509_CERTIFICATE 1

// Supports -[CWVNavigationDelegate handleUnsafeURLWithHandler:].
#define IOS_WEB_VIEW_SUPPORTS_CWV_UNSAFE_URL_HANDLER 1

// Supports -[CWVWebView addMessageHandler:forCommand:] and
// -[CWVWebView removeMessageHandlerForCommand] APIs.
#define IOS_WEB_VIEW_SUPPORTS_MODERN_JS_MESSAGE_HANDLERS 1

// Supports -[CWVWebView evaluateJavaScript:completion:].
#define IOS_WEB_VIEW_SUPPORTS_MODERN_JS_EVALUATION 1

// Supports -[CWVWebViewConfiguration leakCheckService].
#define IOS_WEB_VIEW_SUPPORTS_LEAK_CHECK_SERVICE 1

// Supports -[CWVWebViewConfiguration reuseCheckService].
#define IOS_WEB_VIEW_SUPPORTS_REUSE_CHECK_SERVICE 1

// Supports -[CWVWeakCheckUtils isPasswordWeak:].
#define IOS_WEB_VIEW_SUPPORTS_CWV_WEAK_CHECK_UTILS 1

// Supports -[CWVUserContentController addMessageHandler:forCommand:] and
// -[CWVUserContentController removeMessageHandlerForCommand] APIs.
#define IOS_WEB_VIEW_SUPPORTS_USER_CONTENT_CONTROLLER_MESSAGE_HANDLERS 1

// Supports -[CWVNavigationDelegate
// webView:decidePolicyForNavigationAction:decisionHandler:] and
// -[CWVNavigationDelegate
// webView:decidePolicyForNavigationResponse:decisionHandler:] APIs.
#define IOS_WEB_VIEW_SUPPORTS_ASYNCCHRONOUS_POLICY_DECISION_HANDLER 1

// Supports -[CWVUserScript initWithSource:forMainFrameOnly:].
#define IOS_WEB_VIEW_SUPPORTS_INSTALLING_USER_SCRIPTS_INTO_ALL_FRAMES 1

// Allows the usage of CWVFindInPageController for find in page sessions.
#define IOS_WEB_VIEW_SUPPORTS_FIND_IN_PAGE_SESSIONS 1

// Supports +[CWVWebView webInspectorEnabled].
#define IOS_WEB_VIEW_SUPPORTS_WEB_INSPECTOR_API 1

// Supports +[CWVWebView useOptimizedSessionStorage].
#define IOS_WEB_VIEW_SUPPORTS_OPTIMIZED_STORAGE 1

// Supports CWVOmniboxInput.
#define IOS_WEB_VIEW_SUPPORTS_OMNIBOX_INPUT 1

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_DEFINES_H_
