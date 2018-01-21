// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NeuralNetworkContext_h
#define NeuralNetworkContext_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"

namespace blink {

class Model;
class Compilation;
class Execution;

class NavigatorML;

class NeuralNetworkContext final
    : public ScriptWrappable,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NeuralNetworkContext);
  USING_PRE_FINALIZER(NeuralNetworkContext, Dispose);

 public:
  // Operand types.
  static const unsigned long kFloat32 = 0;
  static const unsigned long kInt32 = 1;
  static const unsigned long kUint32 = 2;
  static const unsigned long kTensorFloat32 = 3;
  static const unsigned long kTensorInt32 = 4;
  static const unsigned long kTensorQuant8Asymm = 5;

  // Operation types.
  static const unsigned long kAdd = 0;
  static const unsigned long kAveragePool2D = 1;
  static const unsigned long kConcatenation = 2;
  static const unsigned long kConv2D = 3;
  static const unsigned long kDepthwiseConv2D = 4;
  static const unsigned long kDepthToSpace = 5;
  static const unsigned long kDequantize = 6;
  static const unsigned long kEmbeddingLookup = 7;
  static const unsigned long kFloor = 8;
  static const unsigned long kFullyConnected = 9;
  static const unsigned long kHashtableLookup = 10;
  static const unsigned long kL2Normalization = 11;
  static const unsigned long kL2Pool2D = 12;
  static const unsigned long kLocalResponseNormalization = 13;
  static const unsigned long kLogistic = 14;
  static const unsigned long kLshProjection = 15;
  static const unsigned long kLstm = 16;
  static const unsigned long kMaxPool2D = 17;
  static const unsigned long kMul = 18;
  static const unsigned long kRelu = 19;
  static const unsigned long kRelu1 = 20;
  static const unsigned long kRelu6 = 21;
  static const unsigned long kReshape = 22;
  static const unsigned long kResizeBilinear = 23;
  static const unsigned long kRnn = 24;
  static const unsigned long kSoftmax = 25;
  static const unsigned long kSpaceToDepth = 26;
  static const unsigned long kSvdf = 27;
  static const unsigned long kTanh = 28;

  // Fused activation function types.
  static const unsigned long kFusedNone = 0;
  static const unsigned long kFusedRelu = 1;
  static const unsigned long kFusedRelu1 = 2;
  static const unsigned long kFusedRelu6 = 3;

  // Implicit padding algorithms.
  static const unsigned long kPaddingSame = 1;
  static const unsigned long kPaddingValid = 2;

  // Execution preferences.
  static const unsigned long kPreferLowPower = 0;
  static const unsigned long kPreferFastSingleAnswer = 1;
  static const unsigned long kPreferSustainedSpeed = 2;

 public:
  NeuralNetworkContext(NavigatorML*);
  ~NeuralNetworkContext() override;

  Model* createModel(ExceptionState&);
  Compilation* createCompilation(Model*, ExceptionState&);
  Execution* createExecution(Compilation*, ExceptionState&);

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Dispose();

  // Interface required by garbage collection.
  void Trace(blink::Visitor*) override;
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 private:
  TraceWrapperMember<NavigatorML> navigator_ml_;
};

}  // namespace blink

#endif  // NFC_h