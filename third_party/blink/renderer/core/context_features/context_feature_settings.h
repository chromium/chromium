// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTEXT_FEATURES_CONTEXT_FEATURE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTEXT_FEATURES_CONTEXT_FEATURE_SETTINGS_H_

#include "third_party/blink/public/web/web_v8_features.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;

// ContextFeatureSettings attaches to an ExecutionContext some basic flags
// pertaining to the enabled/disabled state of any platform API features which
// are gated behind a ContextEnabled extended attribute in IDL.
class CORE_EXPORT ContextFeatureSettings final
    : public GarbageCollected<ContextFeatureSettings>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  enum class CreationMode { kCreateIfNotExists, kDontCreateIfNotExists };

  explicit ContextFeatureSettings(ExecutionContext&);

  // Returns the ContextFeatureSettings for an ExecutionContext. If one does not
  // already exist for the given context, one is created.
  static ContextFeatureSettings* From(ExecutionContext*, CreationMode);

  // Checks that this process has permissions to enable MojoJS on a per-context
  // basis (see `WebV8Features::AllowMojoJSPerContextForProcess()`). Crashes
  // via a `CHECK()` if the permission check fails.
  static void CrashIfMojoJSNotAllowed();

  // ContextEnabled=MojoJS feature
  void EnableMojoJS(bool enable) { enable_mojo_js_ = enable; }
  bool isMojoJSEnabled() const;

  // ContextEnabled=MojoJSFileSystemAccessHelper
  void EnableMojoJSFileSystemAccessHelper(bool enable) {
    DCHECK(enable_mojo_js_);
    enable_mojo_js_file_system_access_helper_ = enable;
  }
  bool isMojoJSFileSystemAccessHelperEnabled() const {
    return enable_mojo_js_file_system_access_helper_;
  }

  enum class UnboundedElementAuth {
    kDenied,
    kAllowedOpenWeb,
    // A non-privileged frame, allowed by permissions policy, whose main frame
    // is privileged. Unlike kAllowedOpenWeb, this does not require user
    // activation.
    kAllowedByPrivilegedMainFrame,
    kAllowedPrivileged,
  };

  static UnboundedElementAuth GetUnboundedElementAuth(
      const ExecutionContext* context);

  using UnboundedElementPrivilege = WebV8Features::UnboundedElementPrivilege;

  // ContextEnabled=UnboundedElement
  void EnableUnboundedElement(UnboundedElementPrivilege privilege) {
    enable_unbounded_element_ = true;
    unbounded_element_privilege_ = privilege;
  }
  bool isUnboundedElementEnabled() const;

  void Trace(Visitor*) const override;

 private:
  bool enable_mojo_js_ = false;
  bool enable_mojo_js_file_system_access_helper_ = false;
  bool enable_unbounded_element_ = false;
  UnboundedElementPrivilege unbounded_element_privilege_ =
      UnboundedElementPrivilege::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTEXT_FEATURES_CONTEXT_FEATURE_SETTINGS_H_
