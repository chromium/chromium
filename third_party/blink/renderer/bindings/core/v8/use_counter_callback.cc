// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/use_counter_callback.h"

#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void UseCounterCallback(v8::Isolate* isolate,
                        v8::Isolate::UseCounterFeature feature) {
  if (V8PerIsolateData::From(isolate)->IsUseCounterDisabled())
    return;

  WebFeature blink_feature;
  bool deprecated = false;
  switch (feature) {
    case v8::Isolate::kUseAsm:
      blink_feature = WebFeature::kUseAsm;
      break;
    case v8::Isolate::kWebAssemblyInstantiation:
      blink_feature = WebFeature::kWebAssemblyInstantiation;
      break;
    case v8::Isolate::kBreakIterator:
      blink_feature = WebFeature::kBreakIterator;
      break;
    case v8::Isolate::kSloppyMode:
      blink_feature = WebFeature::kV8SloppyMode;
      break;
    case v8::Isolate::kStrictMode:
      blink_feature = WebFeature::kV8StrictMode;
      break;
    case v8::Isolate::kRegExpPrototypeStickyGetter:
      blink_feature = WebFeature::kV8RegExpPrototypeStickyGetter;
      break;
    case v8::Isolate::kRegExpPrototypeToString:
      blink_feature = WebFeature::kV8RegExpPrototypeToString;
      break;
    case v8::Isolate::kRegExpPrototypeUnicodeGetter:
      blink_feature = WebFeature::kV8RegExpPrototypeUnicodeGetter;
      break;
    case v8::Isolate::kHtmlCommentInExternalScript:
      blink_feature = WebFeature::kV8HTMLCommentInExternalScript;
      break;
    case v8::Isolate::kHtmlComment:
      blink_feature = WebFeature::kV8HTMLComment;
      break;
    case v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition:
      blink_feature = WebFeature::kV8SloppyModeBlockScopedFunctionRedefinition;
      break;
    case v8::Isolate::kForInInitializer:
      blink_feature = WebFeature::kV8ForInInitializer;
      break;
    case v8::Isolate::kArraySpeciesModified:
      blink_feature = WebFeature::kV8ArraySpeciesModified;
      break;
    case v8::Isolate::kArrayPrototypeConstructorModified:
      blink_feature = WebFeature::kV8ArrayPrototypeConstructorModified;
      break;
    case v8::Isolate::kArrayInstanceConstructorModified:
      blink_feature = WebFeature::kV8ArrayInstanceConstructorModified;
      break;
    case v8::Isolate::kDecimalWithLeadingZeroInStrictMode:
      blink_feature = WebFeature::kV8DecimalWithLeadingZeroInStrictMode;
      break;
    case v8::Isolate::kLegacyDateParser:
      blink_feature = WebFeature::kV8LegacyDateParser;
      break;
    case v8::Isolate::kDefineGetterOrSetterWouldThrow:
      blink_feature = WebFeature::kV8DefineGetterOrSetterWouldThrow;
      break;
    case v8::Isolate::kFunctionConstructorReturnedUndefined:
      blink_feature = WebFeature::kV8FunctionConstructorReturnedUndefined;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy:
      blink_feature = WebFeature::kV8AssigmentExpressionLHSIsCallInSloppy;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInStrict:
      blink_feature = WebFeature::kV8AssigmentExpressionLHSIsCallInStrict;
      break;
    case v8::Isolate::kPromiseConstructorReturnedUndefined:
      blink_feature = WebFeature::kV8PromiseConstructorReturnedUndefined;
      break;
    case v8::Isolate::kErrorCaptureStackTrace:
      blink_feature = WebFeature::kV8ErrorCaptureStackTrace;
      break;
    case v8::Isolate::kErrorPrepareStackTrace:
      blink_feature = WebFeature::kV8ErrorPrepareStackTrace;
      break;
    case v8::Isolate::kErrorStackTraceLimit:
      blink_feature = WebFeature::kV8ErrorStackTraceLimit;
      break;
    case v8::Isolate::kIndexAccessor:
      blink_feature = WebFeature::kV8IndexAccessor;
      break;
    case v8::Isolate::kDeoptimizerDisableSpeculation:
      blink_feature = WebFeature::kV8DeoptimizerDisableSpeculation;
      break;
    case v8::Isolate::kFunctionTokenOffsetTooLongForToString:
      blink_feature = WebFeature::kV8FunctionTokenOffsetTooLongForToString;
      break;
    case v8::Isolate::kWasmSharedMemory:
      blink_feature = WebFeature::kV8WasmSharedMemory;
      break;
    case v8::Isolate::kWasmThreadOpcodes:
      blink_feature = WebFeature::kV8WasmThreadOpcodes;
      break;
    case v8::Isolate::kWasmSimdOpcodes:
      blink_feature = WebFeature::kV8WasmSimdOpcodes;
      break;
    case v8::Isolate::kCollator:
      blink_feature = WebFeature::kCollator;
      break;
    case v8::Isolate::kNumberFormat:
      blink_feature = WebFeature::kNumberFormat;
      break;
    case v8::Isolate::kDateTimeFormat:
      blink_feature = WebFeature::kDateTimeFormat;
      break;
    case v8::Isolate::kPluralRules:
      blink_feature = WebFeature::kPluralRules;
      break;
    case v8::Isolate::kRelativeTimeFormat:
      blink_feature = WebFeature::kRelativeTimeFormat;
      break;
    case v8::Isolate::kLocale:
      blink_feature = WebFeature::kLocale;
      break;
    case v8::Isolate::kListFormat:
      blink_feature = WebFeature::kListFormat;
      break;
    case v8::Isolate::kSegmenter:
      blink_feature = WebFeature::kSegmenter;
      break;
    case v8::Isolate::kStringLocaleCompare:
      blink_feature = WebFeature::kStringLocaleCompare;
      break;
    case v8::Isolate::kStringToLocaleLowerCase:
      blink_feature = WebFeature::kStringToLocaleLowerCase;
      break;
    case v8::Isolate::kNumberToLocaleString:
      blink_feature = WebFeature::kNumberToLocaleString;
      break;
    case v8::Isolate::kDateToLocaleString:
      blink_feature = WebFeature::kDateToLocaleString;
      break;
    case v8::Isolate::kDateToLocaleDateString:
      blink_feature = WebFeature::kDateToLocaleDateString;
      break;
    case v8::Isolate::kDateToLocaleTimeString:
      blink_feature = WebFeature::kDateToLocaleTimeString;
      break;
    case v8::Isolate::kAttemptOverrideReadOnlyOnPrototypeSloppy:
      blink_feature = WebFeature::kV8AttemptOverrideReadOnlyOnPrototypeSloppy;
      break;
    case v8::Isolate::kAttemptOverrideReadOnlyOnPrototypeStrict:
      blink_feature = WebFeature::kV8AttemptOverrideReadOnlyOnPrototypeStrict;
      break;
    case v8::Isolate::kRegExpMatchIsTrueishOnNonJSRegExp:
      blink_feature = WebFeature::kV8RegExpMatchIsTrueishOnNonJSRegExp;
      break;
    case v8::Isolate::kRegExpMatchIsFalseishOnJSRegExp:
      blink_feature = WebFeature::kV8RegExpMatchIsFalseishOnJSRegExp;
      break;
    case v8::Isolate::kStringNormalize:
      blink_feature = WebFeature::kV8StringNormalize;
      break;
    case v8::Isolate::kCallSiteAPIGetFunctionSloppyCall:
      blink_feature = WebFeature::kV8CallSiteAPIGetFunctionSloppyCall;
      break;
    case v8::Isolate::kCallSiteAPIGetThisSloppyCall:
      blink_feature = WebFeature::kV8CallSiteAPIGetThisSloppyCall;
      break;
    case v8::Isolate::kRegExpExecCalledOnSlowRegExp:
      blink_feature = WebFeature::kV8RegExpExecCalledOnSlowRegExp;
      break;
    case v8::Isolate::kRegExpReplaceCalledOnSlowRegExp:
      blink_feature = WebFeature::kV8RegExpReplaceCalledOnSlowRegExp;
      break;
    case v8::Isolate::kSharedArrayBufferConstructed: {
      ExecutionContext* current_execution_context =
          CurrentExecutionContext(isolate);
      if (!current_execution_context) {
        // This callback can be called in a setup where it is not possible to
        // retrieve the current ExecutionContext, e.g. when a shared WebAssembly
        // memory grew on a concurrent worker, and the interrupt that should
        // take care of growing the WebAssembly memory on the current memory was
        // triggered within the execution of a regular expression.
        blink_feature = WebFeature::kV8SharedArrayBufferConstructed;
        break;
      }
      bool is_cross_origin_isolated =
          current_execution_context->CrossOriginIsolatedCapability();
      String protocol =
          current_execution_context->GetSecurityOrigin()->Protocol();
      bool scheme_allows_sab =
          SchemeRegistry::ShouldTreatURLSchemeAsAllowingSharedArrayBuffers(
              protocol);
      bool is_extension_scheme =
          CommonSchemeRegistry::IsExtensionScheme(protocol.Ascii());

      if (!is_cross_origin_isolated && is_extension_scheme) {
        DCHECK(scheme_allows_sab);
        blink_feature = WebFeature::
            kV8SharedArrayBufferConstructedInExtensionWithoutIsolation;
        deprecated = true;
      } else if (is_cross_origin_isolated || scheme_allows_sab) {
        blink_feature = WebFeature::kV8SharedArrayBufferConstructed;
      } else {
        // File an issue. It is performance critical to only file the issue once
        // per context.
        if (!current_execution_context
                 ->has_filed_shared_array_buffer_creation_issue()) {
          current_execution_context->FileSharedArrayBufferCreationIssue();
        }
        blink_feature =
            WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation;
        deprecated = true;
      }
      break;
    }
    case v8::Isolate::kArrayPrototypeHasElements:
      blink_feature = WebFeature::kV8ArrayPrototypeHasElements;
      break;
    case v8::Isolate::kObjectPrototypeHasElements:
      blink_feature = WebFeature::kV8ObjectPrototypeHasElements;
      break;
    case v8::Isolate::kDisplayNames:
      blink_feature = WebFeature::kDisplayNames;
      break;
    case v8::Isolate::kNumberFormatStyleUnit:
      blink_feature = WebFeature::kNumberFormatStyleUnit;
      break;
    case v8::Isolate::kDateTimeFormatRange:
      blink_feature = WebFeature::kDateTimeFormatRange;
      break;
    case v8::Isolate::kDateTimeFormatDateTimeStyle:
      blink_feature = WebFeature::kDateTimeFormatDateTimeStyle;
      break;
    case v8::Isolate::kBreakIteratorTypeWord:
      blink_feature = WebFeature::kBreakIteratorTypeWord;
      break;
    case v8::Isolate::kBreakIteratorTypeLine:
      blink_feature = WebFeature::kBreakIteratorTypeLine;
      break;
    case v8::Isolate::kInvalidatedArrayBufferDetachingProtector:
      blink_feature = WebFeature::kV8InvalidatedArrayBufferDetachingProtector;
      break;
    case v8::Isolate::kInvalidatedArrayConstructorProtector:
      blink_feature = WebFeature::kV8InvalidatedArrayConstructorProtector;
      break;
    case v8::Isolate::kInvalidatedArrayIteratorLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedArrayIteratorLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedArraySpeciesLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedArraySpeciesLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedIsConcatSpreadableLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedIsConcatSpreadableLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedMapIteratorLookupChainProtector:
      blink_feature = WebFeature::kV8InvalidatedMapIteratorLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedNoElementsProtector:
      blink_feature = WebFeature::kV8InvalidatedNoElementsProtector;
      break;
    case v8::Isolate::kInvalidatedPromiseHookProtector:
      blink_feature = WebFeature::kV8InvalidatedPromiseHookProtector;
      break;
    case v8::Isolate::kInvalidatedPromiseResolveLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedPromiseResolveLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedPromiseSpeciesLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedPromiseSpeciesLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedPromiseThenLookupChainProtector:
      blink_feature = WebFeature::kV8InvalidatedPromiseThenLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedRegExpSpeciesLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedRegExpSpeciesLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedSetIteratorLookupChainProtector:
      blink_feature = WebFeature::kV8InvalidatedSetIteratorLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedStringIteratorLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedStringIteratorLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedStringLengthOverflowLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedStringLengthOverflowLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedTypedArraySpeciesLookupChainProtector:
      blink_feature =
          WebFeature::kV8InvalidatedTypedArraySpeciesLookupChainProtector;
      break;
    case v8::Isolate::kInvalidatedNumberStringNotRegexpLikeProtector:
      blink_feature =
          WebFeature::kV8InvalidatedNumberStringNotRegexpLikeProtector;
      break;
    case v8::Isolate::kVarRedeclaredCatchBinding:
      blink_feature = WebFeature::kV8VarRedeclaredCatchBinding;
      break;
    case v8::Isolate::kWasmRefTypes:
      blink_feature = WebFeature::kV8WasmRefTypes;
      break;
    case v8::Isolate::kWasmExceptionHandling:
      blink_feature = WebFeature::kV8WasmExceptionHandling;
      break;
    case v8::Isolate::kFunctionPrototypeArguments:
      blink_feature = WebFeature::kV8FunctionPrototypeArguments;
      break;
    case v8::Isolate::kFunctionPrototypeCaller:
      blink_feature = WebFeature::kV8FunctionPrototypeCaller;
      break;
    case v8::Isolate::kTurboFanOsrCompileStarted:
      blink_feature = WebFeature::kV8TurboFanOsrCompileStarted;
      break;
    case v8::Isolate::kAsyncStackTaggingCreateTaskCall:
      blink_feature = WebFeature::kV8AsyncStackTaggingCreateTaskCall;
      break;
    case v8::Isolate::kCompileHintsMagicAll:
      blink_feature = WebFeature::kV8CompileHintsMagicAll;
      break;
    case v8::Isolate::kWasmMemory64:
      blink_feature = WebFeature::kV8WasmMemory64;
      break;
    case v8::Isolate::kWasmMultiMemory:
      blink_feature = WebFeature::kV8WasmMultiMemory;
      break;
    case v8::Isolate::kWasmGC:
      blink_feature = WebFeature::kV8WasmGC;
      break;
    case v8::Isolate::kWasmImportedStrings:
      blink_feature = WebFeature::kV8WebAssemblyJSStringBuiltins;
      break;
    case v8::Isolate::kSourceMappingUrlMagicCommentAtSign:
      blink_feature = WebFeature::kSourceMappingUrlMagicCommentAtSign;
      break;
    case v8::Isolate::kTemporalObject:
      blink_feature = WebFeature::kV8TemporalObject;
      break;
    case v8::Isolate::kWasmModuleCompilation:
      blink_feature = WebFeature::kWebAssemblyModuleCompilation;
      break;
    case v8::Isolate::kInvalidatedNoUndetectableObjectsProtector:
      blink_feature = WebFeature::kV8InvalidatedNoUndetectableObjectsProtector;
      break;
    case v8::Isolate::kWasmJavaScriptPromiseIntegration:
      blink_feature = WebFeature::kV8WasmJavaScriptPromiseIntegration;
      break;
    case v8::Isolate::kWasmReturnCall:
      blink_feature = WebFeature::kV8WasmReturnCall;
      break;
    case v8::Isolate::kWasmExtendedConst:
      blink_feature = WebFeature::kV8WasmExtendedConst;
      break;
    case v8::Isolate::kWasmRelaxedSimd:
      blink_feature = WebFeature::kV8WasmRelaxedSimd;
      break;
    case v8::Isolate::kWasmTypeReflection:
      blink_feature = WebFeature::kV8WasmTypeReflection;
      break;
    case v8::Isolate::kWasmExnRef:
      blink_feature = WebFeature::kV8WasmExnRef;
      break;
    case v8::Isolate::kWasmTypedFuncRef:
      blink_feature = WebFeature::kV8WasmTypedFuncRef;
      break;
    case v8::Isolate::kDocumentAllLegacyCall:
      blink_feature = WebFeature::kV8DocumentAllLegacyCall;
      break;
    case v8::Isolate::kDocumentAllLegacyConstruct:
      blink_feature = WebFeature::kV8DocumentAllLegacyConstruct;
      break;
    case v8::Isolate::kDurationFormat:
      blink_feature = WebFeature::kDurationFormat;
      break;
    case v8::Isolate::kConsoleContext:
      blink_feature = WebFeature::kV8ConsoleContext;
      break;
    default:
      // This can happen if V8 has added counters that this version of Blink
      // does not know about. It's harmless.
      return;
  }
  if (deprecated) {
    Deprecation::CountDeprecation(CurrentExecutionContext(isolate),
                                  blink_feature);
  } else {
    UseCounter::Count(CurrentExecutionContext(isolate), blink_feature);
  }
}

}  // namespace blink
