// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPES_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPES_UTIL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class ExecutionContext;
class ExceptionState;
class Node;
class StringOrTrustedHTML;
class StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL;
class StringOrTrustedScript;
class StringOrTrustedScriptURL;

enum class SpecificTrustedType {
  kNone,
  kTrustedHTML,
  kTrustedScript,
  kTrustedScriptURL,
};

String CORE_EXPORT GetStringFromTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&,
    const ExecutionContext*,
    ExceptionState&);

String CORE_EXPORT GetStringFromTrustedTypeWithoutCheck(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&);

String CORE_EXPORT GetStringFromSpecificTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&,
    SpecificTrustedType,
    const ExecutionContext*,
    ExceptionState&);

String CORE_EXPORT GetStringFromSpecificTrustedType(const String&,
                                                    SpecificTrustedType,
                                                    const ExecutionContext*,
                                                    ExceptionState&);

String CORE_EXPORT GetStringFromTrustedHTML(StringOrTrustedHTML,
                                            const ExecutionContext*,
                                            ExceptionState&);

String GetStringFromTrustedHTML(const String&,
                                const ExecutionContext*,
                                ExceptionState&);

String CORE_EXPORT GetStringFromTrustedScript(StringOrTrustedScript,
                                              const ExecutionContext*,
                                              ExceptionState&);

String GetStringFromTrustedScript(const String&,
                                  const ExecutionContext*,
                                  ExceptionState&);

String CORE_EXPORT GetStringFromTrustedScriptURL(StringOrTrustedScriptURL,
                                                 const ExecutionContext*,
                                                 ExceptionState&);

// For <script> elements, we need to treat insertion of DOM text nodes
// as equivalent to string assignment. This checks the child-node to be
// inserted and runs all of the Trusted Types checks if it's a text node.
//
// Returns nullptr if the check failed, or the node to use (possibly child)
//         if they succeeded.
Node* TrustedTypesCheckForHTMLScriptElement(Node* child,
                                            Document*,
                                            ExceptionState&);

String TrustedTypesCheckForJavascriptURLinNavigation(const String&, Document*);

// Determine whether a Trusted Types check is needed in this execution context.
//
// Note: All methods above handle this internally and will return success if a
// check is not required. However, in cases where not-required doesn't
// immediately imply "okay" this method can be used.
// Example: To determine whether 'eval' may pass, one needs to also take CSP
// into account.
bool CORE_EXPORT RequireTrustedTypesCheck(const ExecutionContext*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPES_UTIL_H_
