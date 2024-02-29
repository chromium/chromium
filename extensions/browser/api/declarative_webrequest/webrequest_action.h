// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/declarative/declarative_rule.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace base {
class Time;
class Value;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace extensions {
class Extension;
class PermissionHelper;
struct WebRequestData;
}

namespace re2 {
class RE2;
}

namespace extensions {

// Base class for all WebRequestActions of the declarative Web Request API.
class WebRequestAction : public base::RefCounted<WebRequestAction> {
 public:
  // Type identifiers for concrete WebRequestActions. If you add a new type,
  // also update the unittest WebRequestActionTest.GetName, and add a
  // WebRequestActionWithThreadsTest.Permission* unittest.
  enum Type {
    ACTION_CANCEL_REQUEST,
    ACTION_REDIRECT_REQUEST,
    ACTION_REDIRECT_TO_TRANSPARENT_IMAGE,
    ACTION_REDIRECT_TO_EMPTY_DOCUMENT,
    ACTION_REDIRECT_BY_REGEX_DOCUMENT,
    ACTION_SET_REQUEST_HEADER,
    ACTION_REMOVE_REQUEST_HEADER,
    ACTION_ADD_RESPONSE_HEADER,
    ACTION_REMOVE_RESPONSE_HEADER,
    ACTION_IGNORE_RULES,
    ACTION_MODIFY_REQUEST_COOKIE,
    ACTION_MODIFY_RESPONSE_COOKIE,
    ACTION_SEND_MESSAGE_TO_EXTENSION,
  };

  // Strategies for checking host permissions.
  enum HostPermissionsStrategy {
    STRATEGY_NONE,     // Do not check host permissions.
    STRATEGY_DEFAULT,  // Check for host permissions for all URLs
                       // before creating the delta.
    STRATEGY_HOST,     // Check that host permissions match the URL
                       // of the request.
  };

  // Information necessary to decide how to apply a WebRequestAction inside a
  // matching rule. This is passed into HasPermission() and Apply() below as
  // essentially a parameter pack, so the pointers refer to local structures of
  // whatever function is calling one of those methods.
  struct ApplyInfo {
    raw_ptr<PermissionHelper> permission_helper;
    const raw_ref<const WebRequestData> request_data;
    bool crosses_incognito;
    // Modified by each applied action:
    raw_ptr<std::list<extension_web_request_api_helpers::EventResponseDelta>>
        deltas;
    raw_ptr<std::set<std::string>> ignored_tags;
  };

  int stages() const {
    return stages_;
  }

  Type type() const {
    return type_;
  }

  // Compares the Type of two WebRequestActions, needs to be overridden for
  // parameterized types.
  virtual bool Equals(const WebRequestAction* other) const;

  // Return the JavaScript type name corresponding to type(). If there are
  // more names, they are returned separated by a colon.
  virtual std::string GetName() const = 0;

  int minimum_priority() const {
    return minimum_priority_;
  }

  HostPermissionsStrategy host_permissions_strategy() const {
    return host_permissions_strategy_;
  }

  // Returns whether the specified extension has permission to execute this
  // action on |request|. Checks the host permission if the host permissions
  // strategy is STRATEGY_DEFAULT.
  // |apply_info->permission_helper| may only be nullptr for during testing, in
  // which case host permissions are ignored. |crosses_incognito| specifies
  // whether the request comes from a different profile than |extension_id|
  // but was processed because the extension is in spanning mode.
  bool HasPermission(ApplyInfo* apply_info,
                     const ExtensionId& extension_id) const;

  // Factory method that instantiates a concrete WebRequestAction
  // implementation according to |json_action|, the representation of the
  // WebRequestAction as received from the extension API.
  // Sets |error| and returns NULL in case of a semantic error that cannot
  // be caught by schema validation. Sets |bad_message| and returns NULL
  // in case the input is syntactically unexpected.
  static scoped_refptr<const WebRequestAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::Dict& json_action,
      std::string* error,
      bool* bad_message);

  // Returns a description of the modification to the request caused by
  // this action.
  virtual std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const = 0;

  // Applies this action to a request, recording the results into
  // apply_info.deltas.
  void Apply(const ExtensionId& extension_id,
             base::Time extension_install_time,
             ApplyInfo* apply_info) const;

 protected:
  friend class base::RefCounted<WebRequestAction>;
  virtual ~WebRequestAction();
  WebRequestAction(int stages,
                   Type type,
                   int minimum_priority,
                   HostPermissionsStrategy strategy);

 private:
  // A bit vector representing a set of extensions::RequestStage during which
  // the condition can be tested.
  const int stages_;

  const Type type_;

  // The minimum priority of rules that may be evaluated after the rule
  // containing this action.
  const int minimum_priority_;

  // Defaults to STRATEGY_DEFAULT.
  const HostPermissionsStrategy host_permissions_strategy_;
};

using WebRequestActionSet = DeclarativeActionSet<WebRequestAction>;

//
// The following are concrete actions.
//

// Action that instructs to cancel a network request.
class WebRequestCancelAction : public WebRequestAction {
 public:
  WebRequestCancelAction();

  WebRequestCancelAction(const WebRequestCancelAction&) = delete;
  WebRequestCancelAction& operator=(const WebRequestCancelAction&) = delete;

  // Implementation of WebRequestAction:
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestCancelAction() override;
};

// Action that instructs to redirect a network request.
class WebRequestRedirectAction : public WebRequestAction {
 public:
  explicit WebRequestRedirectAction(const GURL& redirect_url);

  WebRequestRedirectAction(const WebRequestRedirectAction&) = delete;
  WebRequestRedirectAction& operator=(const WebRequestRedirectAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRedirectAction() override;

  GURL redirect_url_;  // Target to which the request shall be redirected.
};

// Action that instructs to redirect a network request to a transparent image.
class WebRequestRedirectToTransparentImageAction : public WebRequestAction {
 public:
  WebRequestRedirectToTransparentImageAction();

  WebRequestRedirectToTransparentImageAction(
      const WebRequestRedirectToTransparentImageAction&) = delete;
  WebRequestRedirectToTransparentImageAction& operator=(
      const WebRequestRedirectToTransparentImageAction&) = delete;

  // Implementation of WebRequestAction:
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRedirectToTransparentImageAction() override;
};


// Action that instructs to redirect a network request to an empty document.
class WebRequestRedirectToEmptyDocumentAction : public WebRequestAction {
 public:
  WebRequestRedirectToEmptyDocumentAction();

  WebRequestRedirectToEmptyDocumentAction(
      const WebRequestRedirectToEmptyDocumentAction&) = delete;
  WebRequestRedirectToEmptyDocumentAction& operator=(
      const WebRequestRedirectToEmptyDocumentAction&) = delete;

  // Implementation of WebRequestAction:
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRedirectToEmptyDocumentAction() override;
};

// Action that instructs to redirect a network request.
class WebRequestRedirectByRegExAction : public WebRequestAction {
 public:
  // The |to_pattern| has to be passed in RE2 syntax with the exception that
  // capture groups are referenced in Perl style ($1, $2, ...).
  explicit WebRequestRedirectByRegExAction(
      std::unique_ptr<re2::RE2> from_pattern,
      const std::string& to_pattern);

  WebRequestRedirectByRegExAction(const WebRequestRedirectByRegExAction&) =
      delete;
  WebRequestRedirectByRegExAction& operator=(
      const WebRequestRedirectByRegExAction&) = delete;

  // Conversion of capture group styles between Perl style ($1, $2, ...) and
  // RE2 (\1, \2, ...).
  static std::string PerlToRe2Style(const std::string& perl);

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRedirectByRegExAction() override;

  std::unique_ptr<re2::RE2> from_pattern_;
  std::string to_pattern_;
};

// Action that instructs to set a request header.
class WebRequestSetRequestHeaderAction : public WebRequestAction {
 public:
  WebRequestSetRequestHeaderAction(const std::string& name,
                                   const std::string& value);

  WebRequestSetRequestHeaderAction(const WebRequestSetRequestHeaderAction&) =
      delete;
  WebRequestSetRequestHeaderAction& operator=(
      const WebRequestSetRequestHeaderAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestSetRequestHeaderAction() override;

  std::string name_;
  std::string value_;
};

// Action that instructs to remove a request header.
class WebRequestRemoveRequestHeaderAction : public WebRequestAction {
 public:
  explicit WebRequestRemoveRequestHeaderAction(const std::string& name);

  WebRequestRemoveRequestHeaderAction(
      const WebRequestRemoveRequestHeaderAction&) = delete;
  WebRequestRemoveRequestHeaderAction& operator=(
      const WebRequestRemoveRequestHeaderAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRemoveRequestHeaderAction() override;

  std::string name_;
};

// Action that instructs to add a response header.
class WebRequestAddResponseHeaderAction : public WebRequestAction {
 public:
  WebRequestAddResponseHeaderAction(const std::string& name,
                                    const std::string& value);

  WebRequestAddResponseHeaderAction(const WebRequestAddResponseHeaderAction&) =
      delete;
  WebRequestAddResponseHeaderAction& operator=(
      const WebRequestAddResponseHeaderAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestAddResponseHeaderAction() override;

  std::string name_;
  std::string value_;
};

// Action that instructs to remove a response header.
class WebRequestRemoveResponseHeaderAction : public WebRequestAction {
 public:
  explicit WebRequestRemoveResponseHeaderAction(const std::string& name,
                                                const std::string& value,
                                                bool has_value);

  WebRequestRemoveResponseHeaderAction(
      const WebRequestRemoveResponseHeaderAction&) = delete;
  WebRequestRemoveResponseHeaderAction& operator=(
      const WebRequestRemoveResponseHeaderAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRemoveResponseHeaderAction() override;

  std::string name_;
  std::string value_;
  bool has_value_;
};

// Action that instructs to ignore rules below a certain priority.
class WebRequestIgnoreRulesAction : public WebRequestAction {
 public:
  explicit WebRequestIgnoreRulesAction(int minimum_priority,
                                       const std::string& ignore_tag);

  WebRequestIgnoreRulesAction(const WebRequestIgnoreRulesAction&) = delete;
  WebRequestIgnoreRulesAction& operator=(const WebRequestIgnoreRulesAction&) =
      delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;
  const std::string& ignore_tag() const { return ignore_tag_; }

 private:
  ~WebRequestIgnoreRulesAction() override;

  // Rules are ignored if they have a tag matching |ignore_tag_| and
  // |ignore_tag_| is non-empty.
  std::string ignore_tag_;
};

// Action that instructs to modify (add, edit, remove) a request cookie.
class WebRequestRequestCookieAction : public WebRequestAction {
 public:
  typedef extension_web_request_api_helpers::RequestCookieModification
      RequestCookieModification;

  explicit WebRequestRequestCookieAction(
      RequestCookieModification request_cookie_modification);

  WebRequestRequestCookieAction(const WebRequestRequestCookieAction&) = delete;
  WebRequestRequestCookieAction& operator=(
      const WebRequestRequestCookieAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestRequestCookieAction() override;

  const RequestCookieModification request_cookie_modification_;
};

// Action that instructs to modify (add, edit, remove) a response cookie.
class WebRequestResponseCookieAction : public WebRequestAction {
 public:
  typedef extension_web_request_api_helpers::ResponseCookieModification
      ResponseCookieModification;

  explicit WebRequestResponseCookieAction(
      ResponseCookieModification response_cookie_modification);

  WebRequestResponseCookieAction(const WebRequestResponseCookieAction&) =
      delete;
  WebRequestResponseCookieAction& operator=(
      const WebRequestResponseCookieAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestResponseCookieAction() override;

  const ResponseCookieModification response_cookie_modification_;
};

// Action that triggers the chrome.declarativeWebRequest.onMessage event in
// the background/event/... pages of the extension.
class WebRequestSendMessageToExtensionAction : public WebRequestAction {
 public:
  explicit WebRequestSendMessageToExtensionAction(const std::string& message);

  WebRequestSendMessageToExtensionAction(
      const WebRequestSendMessageToExtensionAction&) = delete;
  WebRequestSendMessageToExtensionAction& operator=(
      const WebRequestSendMessageToExtensionAction&) = delete;

  // Implementation of WebRequestAction:
  bool Equals(const WebRequestAction* other) const override;
  std::string GetName() const override;
  std::optional<extension_web_request_api_helpers::EventResponseDelta>
  CreateDelta(const WebRequestData& request_data,
              const ExtensionId& extension_id,
              const base::Time& extension_install_time) const override;

 private:
  ~WebRequestSendMessageToExtensionAction() override;

  std::string message_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_ACTION_H_
