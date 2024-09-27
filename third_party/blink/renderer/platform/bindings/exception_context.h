// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_

#include <variant>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-exception.h"

namespace blink {

// ExceptionContext stores context information about what Web API throws an
// exception.
//
// Note that ExceptionContext accepts only string literals as its string
// parameters.
class PLATFORM_EXPORT ExceptionContext final {
  DISALLOW_NEW();

 public:
  // Note `class_name` and `property_name` accept only string literals.
  ExceptionContext(v8::ExceptionContext type,
                   const char* class_name,
                   const char* property_name)
      : type_(type), class_name_(class_name), property_name_(property_name) {
#if DCHECK_IS_ON()
    switch (type) {
      case v8::ExceptionContext::kAttributeGet:
      case v8::ExceptionContext::kAttributeSet:
      case v8::ExceptionContext::kOperation:
      case v8::ExceptionContext::kIndexedGetter:
      case v8::ExceptionContext::kIndexedDescriptor:
      case v8::ExceptionContext::kIndexedSetter:
      case v8::ExceptionContext::kIndexedDefiner:
      case v8::ExceptionContext::kIndexedDeleter:
      case v8::ExceptionContext::kIndexedQuery:
      case v8::ExceptionContext::kNamedGetter:
      case v8::ExceptionContext::kNamedDescriptor:
      case v8::ExceptionContext::kNamedSetter:
      case v8::ExceptionContext::kNamedDefiner:
      case v8::ExceptionContext::kNamedDeleter:
      case v8::ExceptionContext::kNamedQuery:
        DCHECK(class_name);
        DCHECK(property_name);
        break;
      case v8::ExceptionContext::kConstructor:
      case v8::ExceptionContext::kNamedEnumerator:
        DCHECK(class_name);
        break;
      case v8::ExceptionContext::kUnknown:
        break;
    }
#endif  // DCHECK_IS_ON()
  }

  ExceptionContext(v8::ExceptionContext type, const char* class_name)
      : ExceptionContext(type, class_name, nullptr) {}

  // Named and indexed property interceptors have a dynamic property name. This
  // variant ensures that the string backing that property name remains alive
  // for the lifetime of the ExceptionContext.
  ExceptionContext(v8::ExceptionContext type,
                   const char* class_name,
                   const String& property_name)
      : type_(type),
        class_name_(class_name),
        property_name_string_(property_name) {}

  ExceptionContext(const ExceptionContext&) = default;
  ExceptionContext(ExceptionContext&&) = default;
  ExceptionContext& operator=(const ExceptionContext&) = default;
  ExceptionContext& operator=(ExceptionContext&&) = default;

  ~ExceptionContext() = default;

  v8::ExceptionContext GetType() const { return type_; }
  const char* GetClassName() const { return class_name_; }
  String GetPropertyName() const {
    DCHECK(!property_name_ || property_name_string_.IsNull());
    return property_name_ ? String(property_name_)
                          : property_name_string_;
  }
  std::variant<const char*, String> GetPropertyNameVariant() const {
    DCHECK(!property_name_ || property_name_string_.IsNull());
    if (property_name_) {
      return property_name_;
    }
    return property_name_string_;
  }

 private:
  v8::ExceptionContext type_;
  const char* class_name_ = nullptr;
  const char* property_name_ = nullptr;
  String property_name_string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_
