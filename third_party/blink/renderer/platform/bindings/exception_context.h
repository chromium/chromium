// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class ExceptionContextType : int16_t {
  kUnknown,  // TODO(crbug.com/270033): Remove this item.
  // IDL Interface, IDL Namespace
  kAttributeGet,
  kAttributeSet,
  kConstructorOperationInvoke,
  kOperationInvoke,
  kIndexedPropertyGetter,
  kIndexedPropertyDescriptor,
  kIndexedPropertySetter,
  kIndexedPropertyDefiner,
  kIndexedPropertyDeleter,
  kIndexedPropertyQuery,
  kNamedPropertyGetter,
  kNamedPropertyDescriptor,
  kNamedPropertySetter,
  kNamedPropertyDefiner,
  kNamedPropertyDeleter,
  kNamedPropertyQuery,
  kNamedPropertyEnumerator,
  // IDL Dictionary
  kDictionaryMemberGet,
};

// ExceptionContext stores context information about what Web API throws an
// exception.
//
// Note that ExceptionContext accepts only string literals as its string
// parameters.
class PLATFORM_EXPORT ExceptionContext final {
  DISALLOW_NEW();

 public:
  // Note `class_name` and `property_name` accept only string literals.
  ExceptionContext(ExceptionContextType type,
                   const char* class_name,
                   const char* property_name)
      : type_(type), class_name_(class_name), property_name_(property_name) {
#if DCHECK_IS_ON()
    switch (type) {
      case ExceptionContextType::kAttributeGet:
      case ExceptionContextType::kAttributeSet:
      case ExceptionContextType::kOperationInvoke:
      case ExceptionContextType::kDictionaryMemberGet:
        DCHECK(class_name);
        DCHECK(property_name);
        break;
      case ExceptionContextType::kConstructorOperationInvoke:
      case ExceptionContextType::kNamedPropertyEnumerator:
        DCHECK(class_name);
        break;
      case ExceptionContextType::kIndexedPropertyGetter:
      case ExceptionContextType::kIndexedPropertyDescriptor:
      case ExceptionContextType::kIndexedPropertySetter:
      case ExceptionContextType::kIndexedPropertyDefiner:
      case ExceptionContextType::kIndexedPropertyDeleter:
      case ExceptionContextType::kIndexedPropertyQuery:
      case ExceptionContextType::kNamedPropertyGetter:
      case ExceptionContextType::kNamedPropertyDescriptor:
      case ExceptionContextType::kNamedPropertySetter:
      case ExceptionContextType::kNamedPropertyDefiner:
      case ExceptionContextType::kNamedPropertyDeleter:
      case ExceptionContextType::kNamedPropertyQuery:
        // Named and indexed property interceptors go through the constructor
        // variant that takes a const String&, never this one.
        NOTREACHED();
        break;
      case ExceptionContextType::kUnknown:
        break;
    }
#endif  // DCHECK_IS_ON()
  }

  ExceptionContext(ExceptionContextType type, const char* class_name)
      : ExceptionContext(type, class_name, nullptr) {}

  // Named and indexed property interceptors have a dynamic property name. This
  // variant ensures that the string backing that property name remains alive
  // for the lifetime of the ExceptionContext.
  ExceptionContext(ExceptionContextType type,
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

  ExceptionContextType GetType() const { return type_; }
  const char* GetClassName() const { return class_name_; }
  String GetPropertyName() const {
    DCHECK(!property_name_ || property_name_string_.IsNull());
    return property_name_ ? String(property_name_)
                          : property_name_string_;
  }
  int16_t GetArgumentIndex() const { return argument_index_; }

  // This is used for a performance hack to reduce the number of construction
  // and destruction times of ExceptionContext when iterating over properties.
  // Only the generated bindings code is allowed to use this hack.
  void ChangePropertyNameAsOptimizationHack(const char* property_name) {
    DCHECK(property_name_string_.IsNull());
    property_name_ = property_name;
  }

 private:
  ExceptionContextType type_;
  int16_t argument_index_ = 0;
  const char* class_name_ = nullptr;
  const char* property_name_ = nullptr;
  String property_name_string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_
