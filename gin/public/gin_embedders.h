// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_GIN_EMBEDDERS_H_
#define GIN_PUBLIC_GIN_EMBEDDERS_H_

#include <cstdint>

namespace gin {

// The GinEmbedder is used to identify the owner of embedder data stored on
// v8 objects, and is used as in index into the embedder data slots of a
// v8::Isolate.
//
// GinEmbedder is using uint16_t as underlying storage as V8 requires that
// external pointers in embedder fields are at least 2-byte-aligned.
enum GinEmbedder : uint16_t {
  kEmbedderNativeGin,
  kEmbedderBlink,
  kEmbedderPDFium,
  kEmbedderFuchsia,
};

enum EmbedderDataTag : uint16_t {
  // kDeprecatedData is used for data that is already not used anymore but still
  // exists for legacy reasons, e.g. in the implementation of
  // gin::DeprecatedWrappable.
  kDeprecatedData,
  // kDefaultTag can be used by embedders that don't use V8 type tagging, e.g.
  // because they have their own type tagging system, like PDFium.
  kDefaultEmbedderDataTag = kDeprecatedData,
  kBlinkScriptState,
  kGinPerContextData,
};

enum ExternalPointerTypeTag : uint16_t {
  kExternalPointerTypeTagDefaultTag = 0,
  kAppHooksDelegateTag,
  kAuctionV8LoggerTest_TestLazyFillerTag,
  kForDebuggingOnlyBindingsTag,
  kLazyFillerTag,
  kPrivateAggregationBindingsTag,
  kPrivateModelTrainingBindingsTag,
  kRealTimeReportingBindingsTag,
  kRegisterAdBeaconBindingsTag,
  kRegisterAdMacroBindingsTag,
  kReportBindingsTag,
  kSetBidBindingsTag,
  kSetPriorityBindingsTag,
  kSetPrioritySignalsOverrideBindingsTag,
  kSharedStorageBindingsTag,
  kTextConversionHelpersTag,
  kWebIDLCompatTestTag,
  kDeclarativeContentHooksDelegateHandlerCallbackTag,
  kAPIBindingHandlerCallbackTag,
  kAPIBindingEventDataTag,
  kAPIBindingCustomPropertyDataTag,
  kAPIBindingJSUtilUnittestErrorInfoTag,
  kEventEmitterUnittestListenerClosureDataTag,
  kModuleSystemTag,
  kObjectBackedNativeHandlerHandlerFunctionTag,
  kGinInternalCallbackHolderBaseTag,
  kProxyResolverV8ContextTag,
  kThreadDebuggerCommonImplTag,
  kViewTransitionTestDataTag,
  kViewTransitionTestDocumentTag,
  kViewTransitionTestBoolTag,
  kFXJSEFunctionDescriptorTag,
  kFXJSEClassDescriptorTag,
  kV8IsolateTag,
};

}  // namespace gin

#endif  // GIN_PUBLIC_GIN_EMBEDDERS_H_
