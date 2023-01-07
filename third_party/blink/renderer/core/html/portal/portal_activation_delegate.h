// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_ACTIVATION_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_ACTIVATION_DELEGATE_H_

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

class ConsoleLogger;
class ExceptionContext;
class ScriptPromiseResolver;

// Handles the result of portal activation, reporting it in a suitable way.
class PortalActivationDelegate : public GarbageCollectedMixin {
 public:
  // Creates a delegate which reports completion through promise resolution.
  // Copies the metadata from the supplied ExceptionContext.
  static PortalActivationDelegate* ForPromise(ScriptPromiseResolver*,
                                              const ExceptionContext&);

  // Creates a delegate which logs errors to the console.
  static PortalActivationDelegate* ForConsole(ConsoleLogger*);

  // Called when activation is complete.
  virtual void ActivationDidSucceed() = 0;

  // Called when activation fails, with a suggested message.
  virtual void ActivationDidFail(const WTF::String& message) = 0;

  // Called when activation is abandoned without completion, e.g. due to a
  // corrupt state or disconnection.
  virtual void ActivationWasAbandoned() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_ACTIVATION_DELEGATE_H_
