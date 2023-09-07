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

// ExceptionContext stores context information about what Web API throws an
// exception.
//
// Note that ExceptionContext accepts only string literals as its string
// parameters.
class PLATFORM_EXPORT ExceptionContext final {
  DISALLOW_NEW();

 public:
  enum class Context : int16_t {
    kEmpty,
    kUnknown,  // TODO(crbug.com/270033): Remove this item.
    // IDL Interface, IDL Namespace
    kAttributeGet,
    kAttributeSet,
    kConstantGet,
    kConstructorOperationInvoke,
    kOperationInvoke,
    kIndexedPropertyGetter,
    kIndexedPropertyDescriptor,
    kIndexedPropertySetter,
    kIndexedPropertyDefiner,
    kIndexedPropertyDeleter,
    kIndexedPropertyQuery,
    kIndexedPropertyEnumerator,
    kNamedPropertyGetter,
    kNamedPropertyDescriptor,
    kNamedPropertySetter,
    kNamedPropertyDefiner,
    kNamedPropertyDeleter,
    kNamedPropertyQuery,
    kNamedPropertyEnumerator,
    // IDL Dictionary
    kDictionaryMemberGet,
    kDictionaryMemberSet,
    // IDL Callback Function
    kCallbackFunctionConstruct,
    kCallbackFunctionInvoke,
    // IDL Callback Interface
    kCallbackInterfaceOperationInvoke,
    // Operating on a function argument
    kFunctionArgument,
  };

  ExceptionContext() = default;

  // Note `class_name` and `property_name` accept only string literals.
  explicit ExceptionContext(Context context,
                            const char* class_name,
                            const char* property_name)
      : context_(context),
        class_name_(class_name),
        property_name_(property_name) {
#if DCHECK_IS_ON()
    switch (context) {
      case Context::kAttributeGet:
      case Context::kAttributeSet:
      case Context::kConstantGet:
      case Context::kOperationInvoke:
      case Context::kDictionaryMemberGet:
      case Context::kDictionaryMemberSet:
      case Context::kCallbackInterfaceOperationInvoke:
        DCHECK(class_name);
        DCHECK(property_name);
        break;
      case Context::kConstructorOperationInvoke:
      case Context::kIndexedPropertyGetter:
      case Context::kIndexedPropertyDescriptor:
      case Context::kIndexedPropertySetter:
      case Context::kIndexedPropertyDefiner:
      case Context::kIndexedPropertyDeleter:
      case Context::kIndexedPropertyQuery:
      case Context::kIndexedPropertyEnumerator:
      case Context::kNamedPropertyGetter:
      case Context::kNamedPropertyDescriptor:
      case Context::kNamedPropertySetter:
      case Context::kNamedPropertyDefiner:
      case Context::kNamedPropertyDeleter:
      case Context::kNamedPropertyQuery:
      case Context::kNamedPropertyEnumerator:
      case Context::kCallbackFunctionConstruct:
      case Context::kCallbackFunctionInvoke:
        DCHECK(class_name);
        break;
      case Context::kEmpty:
      case Context::kFunctionArgument:
        NOTREACHED();
        break;
      case Context::kUnknown:
        break;
    }
#endif  // DCHECK_IS_ON()
  }

  explicit ExceptionContext(Context context, const char* class_name)
      : ExceptionContext(context, class_name, nullptr) {}

  explicit ExceptionContext(Context context, int16_t argument_index)
      : context_(context), argument_index_(argument_index) {
    DCHECK_EQ(Context::kFunctionArgument, context);
  }

  // Named and indexed property interceptors have a dynamic property name. This
  // variant ensures that the string backing that property name remains alive
  // for the lifetime of the ExceptionContext.
  explicit ExceptionContext(Context context,
                            const char* class_name,
                            const String& property_name)
      : context_(context),
        class_name_(class_name),
        property_name_string_(property_name) {}

  ExceptionContext(const ExceptionContext&) = default;
  ExceptionContext(ExceptionContext&&) = default;
  ExceptionContext& operator=(const ExceptionContext&) = default;
  ExceptionContext& operator=(ExceptionContext&&) = default;

  ~ExceptionContext() = default;

  Context GetContext() const { return context_; }
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
  Context context_ = Context::kEmpty;
  int16_t argument_index_ = 0;
  const char* class_name_ = nullptr;
  const char* property_name_ = nullptr;
  String property_name_string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_CONTEXT_H_
