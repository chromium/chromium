// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"
#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_model.h"
#include "third_party/blink/renderer/modules/ml/ml_model_loader_test_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace blink_mojom = ml::model_loader::mojom::blink;

namespace {

// Helper struct to create faked MLTensors.
struct TensorInfo {
  uint32_t byte_size;
  blink_mojom::DataType data_type;
  WTF::Vector<uint32_t> dimensions;

  blink_mojom::TensorInfoPtr ToMojom() const {
    auto ret = blink_mojom::TensorInfo::New();
    ret->byte_size = byte_size;
    ret->data_type = data_type;
    ret->dimensions = dimensions;
    return ret;
  }

  MLTensor* ToMLTensor() const {
    auto* ret = MakeGarbageCollected<MLTensor>();

    // Intentionally create from `byte_size` and ignore `dimensions`.
    //
    // The test needs to verify safe-guarding logic when `byte_size` doesn't
    // match with `data_type` and `dimensions`.
    auto* array_buffer = DOMArrayBuffer::Create(
        /*number_of_elements*/ byte_size, /*element_size*/ 1u);
    auto* data_view = DOMDataView::Create(array_buffer, 0, byte_size);

    ret->setData(NotShared<DOMArrayBufferView>(data_view));
    ret->setDimensions(dimensions);

    return ret;
  }
};

Vector<uint8_t> CreateByteVector(uint32_t size) {
  return Vector<uint8_t>(size);
}

// A fake MLModel Mojo interface implementation that backs a Blink MLModel
// object.
class FakeMLModel : public blink_mojom::Model {
 public:
  FakeMLModel() = default;
  FakeMLModel(const FakeMLModel&) = delete;
  FakeMLModel(FakeMLModel&&) = delete;
  ~FakeMLModel() override = default;

  using ComputeFn = base::OnceCallback<void(
      const WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>&,
      blink_mojom::Model::ComputeCallback callback)>;

  void Compute(const WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>& input,
               blink_mojom::Model::ComputeCallback callback) override {
    std::move(compute_).Run(input, std::move(callback));
  }

  FakeMLModelLoader::LoadFn CreateFromInfo(
      const std::map<std::string, TensorInfo>& inputs,
      const std::map<std::string, TensorInfo>& outputs) {
    info_ = blink_mojom::ModelInfo::New();

    for (const auto& [name, tensor] : inputs) {
      info_->input_tensor_info.insert(WTF::String(name), tensor.ToMojom());
    }
    for (const auto& [name, tensor] : outputs) {
      info_->output_tensor_info.insert(WTF::String(name), tensor.ToMojom());
    }

    return WTF::BindRepeating(&FakeMLModel::OnCreateModel,
                              // Safe to WTF::Unretained, this method won't be
                              // called after test finishes.
                              WTF::Unretained(this));
  }

  void OnCreateModel(mojo_base::BigBuffer,
                     blink_mojom::ModelLoader::LoadCallback callback) {
    receiver_.reset();
    std::move(callback).Run(blink_mojom::LoadModelResult::kOk,
                            receiver_.BindNewPipeAndPassRemote(),
                            info_->Clone());
  }

  void SetComputeResult(
      const std::map<std::string, WTF::Vector<uint8_t>>& output) {
    WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> ml_output;
    for (const auto& [name, data] : output) {
      ml_output.Set(WTF::String(name), data);
    }
    compute_ = WTF::BindOnce(
        [](WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> output,
           const WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>&,
           blink_mojom::Model::ComputeCallback callback) {
          std::move(callback).Run(blink_mojom::ComputeResult::kOk,
                                  std::move(output));
        },
        ml_output);
  }

  void SetComputeFailure(const blink_mojom::ComputeResult result) {
    compute_ = WTF::BindOnce(
        [](const blink_mojom::ComputeResult result,
           const WTF::HashMap<WTF::String, WTF::Vector<uint8_t>>&,
           blink_mojom::Model::ComputeCallback callback) {
          std::move(callback).Run(result, {});
        },
        result);
  }

 private:
  blink_mojom::ModelInfoPtr info_;
  ComputeFn compute_;

  mojo::Receiver<blink_mojom::Model> receiver_{this};
};
}  // namespace

class MLModelLoaderTest : public testing::Test {
 public:
  MLModelLoaderTest() = default;
  MLModelLoaderTest(const MLModelLoaderTest&) = delete;
  MLModelLoaderTest(MLModelLoaderTest&&) = delete;
  ~MLModelLoaderTest() override = default;

  MLModelLoader* CreateTestLoader(V8TestingScope& scope) {
    ML* ml = MakeGarbageCollected<ML>(scope.GetExecutionContext());

    MLContextOptions* options = MLContextOptions::Create();
    options->setDevicePreference(V8MLDevicePreference::Enum::kCpu);
    options->setPowerPreference(V8MLPowerPreference::Enum::kAuto);
    options->setModelFormat(V8MLModelFormat::Enum::kTflite);

    MLContext* ml_context = ml->createContextSync(
        scope.GetScriptState(), options, scope.GetExceptionState());
    return MLModelLoader::Create(scope.GetScriptState(), ml_context,
                                 scope.GetExceptionState());
  }

  MLModel* ScriptValueToMLModel(const ScriptValue& value) {
    return V8MLModel::ToWrappable(value.GetIsolate(), value.V8Value());
  }

  String ScriptValueToDOMExceptionName(const ScriptValue& value) {
    auto* exception =
        V8DOMException::ToWrappable(value.GetIsolate(), value.V8Value());
    if (!exception) {
      return DOMException::GetErrorName(DOMExceptionCode::kNoError);
    }
    return exception->name();
  }

 protected:
  FakeMLService service_;
  FakeMLModelLoader loader_;
  FakeMLModel model_;
};

TEST_F(MLModelLoaderTest, UnsupportedContextOptions) {
  V8TestingScope scope;
  ScopedSetMLServiceBinder scoped_setup_binder(&service_, scope);
  service_.SetCreateModelLoader(loader_.CreateForUnsupportedContext());

  auto* ml_model_loader = CreateTestLoader(scope);
  ASSERT_TRUE(ml_model_loader);

  auto promise = ml_model_loader->load(scope.GetScriptState(),
                                       DOMArrayBuffer::Create(1u, 1u),
                                       scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  auto tester = ScriptPromiseTester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  EXPECT_EQ(ScriptValueToDOMExceptionName(tester.Value()),
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
}

TEST_F(MLModelLoaderTest, LoadModelAndCompute) {
  V8TestingScope scope;
  ScopedSetMLServiceBinder scoped_setup_binder(&service_, scope);
  service_.SetCreateModelLoader(loader_.CreateFromThis());
  loader_.SetLoad(model_.CreateFromInfo(
      {{"in",
        TensorInfo{
            .byte_size = 16,
            .data_type = blink_mojom::DataType::kUint8,
            .dimensions = WTF::Vector<uint32_t>{4, 4},
        }}},
      {{"out", TensorInfo{
                   .byte_size = 4,
                   .data_type = blink_mojom::DataType::kFloat32,
                   .dimensions = WTF::Vector<uint32_t>{1},
               }}}));

  auto* ml_model_loader = CreateTestLoader(scope);
  ASSERT_TRUE(ml_model_loader);

  auto promise = ml_model_loader->load(scope.GetScriptState(),
                                       DOMArrayBuffer::Create(1u, 1u),
                                       scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  auto tester = ScriptPromiseTester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto* ml_model = ScriptValueToMLModel(tester.Value());
  EXPECT_TRUE(ml_model);

  auto input_info = ml_model->inputs(scope.GetScriptState());
  EXPECT_EQ(input_info.size(), 1u);
  EXPECT_EQ(input_info[0]->name(), "in");
  EXPECT_EQ(input_info[0]->type().AsEnum(), V8MLDataType::Enum::kUint8);
  EXPECT_EQ(input_info[0]->dimensions(), WTF::Vector<uint32_t>({4, 4}));

  auto output_info = ml_model->outputs(scope.GetScriptState());
  EXPECT_EQ(output_info.size(), 1u);
  EXPECT_EQ(output_info[0]->name(), "out");
  EXPECT_EQ(output_info[0]->type().AsEnum(), V8MLDataType::Enum::kFloat32);
  EXPECT_EQ(output_info[0]->dimensions(), WTF::Vector<uint32_t>({1}));

  HeapVector<std::pair<String, Member<MLTensor>>> inputs;
  inputs.emplace_back(WTF::String("in"),
                      TensorInfo{.byte_size = 16,
                                 .data_type = blink_mojom::DataType::kUint8,
                                 .dimensions = WTF::Vector<uint32_t>{4, 4}}
                          .ToMLTensor());
  model_.SetComputeResult({{"out", CreateByteVector(4)}});

  auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                           scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  auto compute_tester =
      ScriptPromiseTester(scope.GetScriptState(), compute_promise);
  compute_tester.WaitUntilSettled();
  EXPECT_TRUE(compute_tester.IsFulfilled());

  const auto& output =
      NativeValueTraits<IDLRecord<IDLString, MLTensor>>::NativeValue(
          compute_tester.Value().GetIsolate(), compute_tester.Value().V8Value(),
          scope.GetExceptionState());
  EXPECT_EQ(output.size(), 1u);
  EXPECT_EQ(output[0].first, "out");
  EXPECT_EQ(output[0].second->dimensions(), Vector<uint32_t>{1});
  EXPECT_EQ(output[0].second->data()->byteLength(), 4u);
}

TEST_F(MLModelLoaderTest, InputMismatch) {
  V8TestingScope scope;
  ScopedSetMLServiceBinder scoped_setup_binder(&service_, scope);
  service_.SetCreateModelLoader(loader_.CreateFromThis());
  loader_.SetLoad(model_.CreateFromInfo(
      {{"in",
        TensorInfo{
            .byte_size = 16,
            .data_type = blink_mojom::DataType::kUint8,
            .dimensions = WTF::Vector<uint32_t>{4, 4},
        }}},
      {{"out", TensorInfo{
                   .byte_size = 4,
                   .data_type = blink_mojom::DataType::kFloat32,
                   .dimensions = WTF::Vector<uint32_t>{1},
               }}}));

  auto* ml_model_loader = CreateTestLoader(scope);
  ASSERT_TRUE(ml_model_loader);

  auto promise = ml_model_loader->load(scope.GetScriptState(),
                                       DOMArrayBuffer::Create(1u, 1u),
                                       scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  auto tester = ScriptPromiseTester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto* ml_model = ScriptValueToMLModel(tester.Value());
  EXPECT_TRUE(ml_model);

  {
    // Number of input doesn't match the model's input.
    HeapVector<std::pair<String, Member<MLTensor>>> inputs;
    inputs.emplace_back(WTF::String("in"),
                        TensorInfo{.byte_size = 16,
                                   .data_type = blink_mojom::DataType::kUint8,
                                   .dimensions = WTF::Vector<uint32_t>{4, 4}}
                            .ToMLTensor());
    inputs.emplace_back(WTF::String("in2"),
                        TensorInfo{.byte_size = 16,
                                   .data_type = blink_mojom::DataType::kUint8,
                                   .dimensions = WTF::Vector<uint32_t>{4, 4}}
                            .ToMLTensor());

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
  }

  {
    // Input byte size mismatch.
    HeapVector<std::pair<String, Member<MLTensor>>> inputs;
    inputs.emplace_back(WTF::String("in"),
                        TensorInfo{.byte_size = 8,
                                   .data_type = blink_mojom::DataType::kUint8,
                                   .dimensions = WTF::Vector<uint32_t>{4, 4}}
                            .ToMLTensor());

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
  }

  {
    // Input name mismatch.
    HeapVector<std::pair<String, Member<MLTensor>>> inputs;
    inputs.emplace_back(WTF::String("invalid-input-name"),
                        TensorInfo{.byte_size = 16,
                                   .data_type = blink_mojom::DataType::kUint8,
                                   .dimensions = WTF::Vector<uint32_t>{4, 4}}
                            .ToMLTensor());

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kDataError));
  }
}

TEST_F(MLModelLoaderTest, ComputeFailures) {
  V8TestingScope scope;
  ScopedSetMLServiceBinder scoped_setup_binder(&service_, scope);
  service_.SetCreateModelLoader(loader_.CreateFromThis());
  loader_.SetLoad(model_.CreateFromInfo(
      {{"in",
        TensorInfo{
            .byte_size = 16,
            .data_type = blink_mojom::DataType::kUint8,
            .dimensions = WTF::Vector<uint32_t>{4, 4},
        }}},
      {{"out", TensorInfo{
                   .byte_size = 4,
                   .data_type = blink_mojom::DataType::kFloat32,
                   .dimensions = WTF::Vector<uint32_t>{1},
               }}}));

  auto* ml_model_loader = CreateTestLoader(scope);
  ASSERT_TRUE(ml_model_loader);

  auto promise = ml_model_loader->load(scope.GetScriptState(),
                                       DOMArrayBuffer::Create(1u, 1u),
                                       scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  auto tester = ScriptPromiseTester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto* ml_model = ScriptValueToMLModel(tester.Value());
  EXPECT_TRUE(ml_model);

  HeapVector<std::pair<String, Member<MLTensor>>> inputs;
  inputs.emplace_back(WTF::String("in"),
                      TensorInfo{.byte_size = 16,
                                 .data_type = blink_mojom::DataType::kUint8,
                                 .dimensions = WTF::Vector<uint32_t>{4, 4}}
                          .ToMLTensor());

  {
    // No computation result (e.g. due to a backend failure).
    model_.SetComputeFailure(blink_mojom::ComputeResult::kUnknownError);

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kOperationError));
  }

  {
    // Output tensor count mismatch.
    model_.SetComputeResult(
        {{"out1", CreateByteVector(4)}, {"out2", CreateByteVector(4)}});

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
  }

  {
    // Output tensor name mismatch.
    model_.SetComputeResult({{"a_different_out_name", CreateByteVector(4)}});

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
  }

  {
    // Output tensor byteLength mismatch.
    model_.SetComputeResult({{"out", CreateByteVector(8)}});

    auto compute_promise = ml_model->compute(scope.GetScriptState(), inputs,
                                             scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    auto compute_tester =
        ScriptPromiseTester(scope.GetScriptState(), compute_promise);
    compute_tester.WaitUntilSettled();
    EXPECT_TRUE(compute_tester.IsRejected());
    EXPECT_EQ(ScriptValueToDOMExceptionName(compute_tester.Value()),
              DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
  }
}

}  // namespace blink
