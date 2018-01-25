// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Compilation_h
#define Compilation_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "services/ml/public/interfaces/neuralnetwork.mojom-blink.h"

namespace blink {

class ExceptionState;
class Execution;
class NavigatorML;

class Model;

class Compilation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Compilation(NavigatorML*);
  ~Compilation() override;

  void setModel(Model*, ExceptionState&);
  void setPreference(int32_t, ExceptionState&);
  ScriptPromise finish(ScriptState*);

  bool IsFinished() {return is_finished_;}
  int32_t GetID() {return id_;}
  Model* GetModel() {return model_;}

  void Trace(blink::Visitor*);
 private:
  void OnCompileDone(ScriptPromiseResolver*, int32_t);
  void OnConnectionError();

  bool is_finished_;
  int32_t id_;
  int32_t preference_;

  ml::mojom::blink::NeuralNetworkPtr service_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  Member<Model> model_;
};

}  // namespace blink

#endif  // Compilation_h