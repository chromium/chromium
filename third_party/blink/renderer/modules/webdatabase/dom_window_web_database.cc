/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/dom_window_web_database.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_database_callback.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

Database* DOMWindowWebDatabase::openDatabase(LocalDOMWindow& window,
                                             const String& name,
                                             const String& version,
                                             const String& display_name,
                                             uint32_t estimated_size,
                                             ExceptionState& exception_state) {
  return openDatabase(window, name, version, display_name, estimated_size,
                      nullptr, exception_state);
}

Database* DOMWindowWebDatabase::openDatabase(
    LocalDOMWindow& window,
    const String& name,
    const String& version,
    const String& display_name,
    uint32_t estimated_size,
    V8DatabaseCallback* creation_callback,
    ExceptionState& exception_state) {
  if (!window.IsCurrentlyDisplayedInFrame())
    return nullptr;

  Database* database = nullptr;
  DatabaseError error = DatabaseError::kNone;
  if (RuntimeEnabledFeatures::DatabaseEnabled(window.GetExecutionContext()) &&
      window.GetSecurityOrigin()->CanAccessDatabase()) {
    if (window.GetSecurityOrigin()->IsLocal())
      UseCounter::Count(window, WebFeature::kFileAccessedDatabase);

    if (!window.GetExecutionContext()->IsSecureContext()) {
      exception_state.ThrowSecurityError(
          "Access to the WebDatabase API is denied in non-secure contexts.");
      return nullptr;
    }

    if (window.IsCrossSiteSubframeIncludingScheme()) {
      exception_state.ThrowSecurityError(
          "Access to the WebDatabase API is denied in third party contexts.");
      return nullptr;
    }

    String error_message;
    database = DatabaseContext::From(window)->OpenDatabase(
        name, version, display_name, creation_callback, error, error_message);
    DCHECK(database || error != DatabaseError::kNone);
    if (error != DatabaseError::kNone)
      DatabaseContext::ThrowExceptionForDatabaseError(error, error_message,
                                                      exception_state);
  } else {
    exception_state.ThrowSecurityError(
        "Access to the WebDatabase API is denied in this context.");
  }

  return database;
}

}  // namespace blink
