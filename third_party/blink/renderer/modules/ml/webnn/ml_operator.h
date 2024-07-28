// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;
class MLOperand;

class MODULES_EXPORT MLOperator : public GarbageCollected<MLOperator> {
 public:
  using OperationSubKind =
      absl::variant<webnn::mojom::blink::ArgMinMax::Kind,
                    webnn::mojom::blink::Conv2d::Kind,
                    webnn::mojom::blink::ElementWiseBinary::Kind,
                    webnn::mojom::blink::ElementWiseUnary::Kind,
                    webnn::mojom::blink::Pool2d::Kind,
                    webnn::mojom::blink::Reduce::Kind,
                    absl::monostate>;

  static String OperatorKindToString(
      webnn::mojom::blink::Operation::Tag kind,
      OperationSubKind sub_kind = absl::monostate{});

  // It is safe for a caller, usually a MLGraphBuidler operation build method,
  // that passes the reference of the options dictionary argument received from
  // Blink to MLOperator constructor and stores it in this object. This is
  // because that WebIDL spec (https://webidl.spec.whatwg.org/#idl-dictionaries)
  // mentiones that "an operation that accepts a dictionary as an argument will
  // perform a one-time conversion from the given ECMAScript value into the
  // dictionary, based on the current properties of the ECMAScript object.
  // Modifications to the dictionary will not be reflected in the corresponding
  // ECMAScript object, and vice-versa". Blink code generator follows the spec
  // and does a deep-copy of the members of an options dictionary, e.g.,
  // MLConv2dOptions::FillMembersFromV8Object, before passing it to a
  // MLGraphBuilder operation build method.
  MLOperator(MLGraphBuilder* builder,
             webnn::mojom::blink::Operation::Tag kind,
             const bindings::DictionaryBase* options,
             OperationSubKind sub_kind = absl::monostate{});

  MLOperator(const MLOperator&) = delete;
  MLOperator& operator=(const MLOperator&) = delete;

  virtual ~MLOperator();

  void Trace(Visitor* visitor) const;

  webnn::mojom::blink::Operation::Tag Kind() const;
  OperationSubKind SubKind() const;
  template <typename MojomKind>
  MojomKind SubKind() const {
    return absl::get<MojomKind>(SubKind());
  }

  const bindings::DictionaryBase* Options() const;
  const HeapVector<Member<const MLOperand>>& Inputs() const;
  const HeapVector<Member<const MLOperand>>& Outputs() const;
  MLGraphBuilder const* Builder() const { return builder_.Get(); }

  // According to WebNN programming model
  // https://www.w3.org/TR/webnn/#programming-model, neural networks are
  // represented as computational graphs of mathematical operators (nodes)
  // connected by operands (edges). This method connects the operator with its
  // input and output operands during a computational graph building session. An
  // operator is only allowed to be connected once.
  void Connect(HeapVector<Member<const MLOperand>> inputs,
               HeapVector<Member<const MLOperand>> outputs);

 private:
  Member<MLGraphBuilder> builder_;
  webnn::mojom::blink::Operation::Tag kind_;

  // The correct type of options_ depends on OperatorKind. For example, if the
  // OperatorKind is kClamp, options_ could static_cast to MLClampOptions.
  Member<const bindings::DictionaryBase> options_;
  OperationSubKind sub_kind_;

  HeapVector<Member<const MLOperand>> inputs_;
  HeapVector<Member<const MLOperand>> outputs_;
};

// TODO: crbug.com/325612086 - Remove all these subclasses. This information
// should all be contained within the respective mojo Operation struct.

class MODULES_EXPORT MLArgMinMaxOperator : public MLOperator {
 public:
  MLArgMinMaxOperator(MLGraphBuilder* builder,
                      OperationSubKind sub_kind,
                      const uint32_t axis,
                      const bindings::DictionaryBase* options);

  MLArgMinMaxOperator(const MLArgMinMaxOperator&) = delete;
  MLArgMinMaxOperator& operator=(const MLArgMinMaxOperator&) = delete;

  ~MLArgMinMaxOperator() override;

  uint32_t Axis() const { return axis_; }

 private:
  uint32_t axis_;
};

class MODULES_EXPORT MLConcatOperator : public MLOperator {
 public:
  MLConcatOperator(MLGraphBuilder* builder,
                   const uint32_t axis,
                   const bindings::DictionaryBase* options);

  MLConcatOperator(const MLConcatOperator&) = delete;
  MLConcatOperator& operator=(const MLConcatOperator&) = delete;

  ~MLConcatOperator() override;

  uint32_t Axis() const;

 private:
  uint32_t axis_;
};

class MODULES_EXPORT MLLstmOperator : public MLOperator {
 public:
  MLLstmOperator(MLGraphBuilder* builder,
                 uint32_t steps,
                 uint32_t hidden_size,
                 const bindings::DictionaryBase* options);

  MLLstmOperator(const MLLstmOperator&) = delete;
  MLLstmOperator& operator=(const MLLstmOperator&) = delete;

  ~MLLstmOperator() override;

  uint32_t steps() const;
  uint32_t hidden_size() const;

 private:
  uint32_t steps_;
  uint32_t hidden_size_;
};

class MODULES_EXPORT MLLstmCellOperator : public MLOperator {
 public:
  MLLstmCellOperator(MLGraphBuilder* builder,
                     uint32_t hidden_size,
                     const bindings::DictionaryBase* options);

  MLLstmCellOperator(const MLLstmCellOperator&) = delete;
  MLLstmCellOperator& operator=(const MLLstmCellOperator&) = delete;

  ~MLLstmCellOperator() override;

  uint32_t hidden_size() const;

 private:
  const uint32_t hidden_size_;
};

class MODULES_EXPORT MLGruOperator : public MLOperator {
 public:
  MLGruOperator(MLGraphBuilder* builder,
                uint32_t steps,
                uint32_t hidden_size,
                const bindings::DictionaryBase* options);

  MLGruOperator(const MLGruOperator&) = delete;
  MLGruOperator& operator=(const MLGruOperator&) = delete;

  ~MLGruOperator() override;

  uint32_t steps() const { return steps_; }
  uint32_t hidden_size() const { return hidden_size_; }

 private:
  const uint32_t steps_;
  const uint32_t hidden_size_;
};

class MODULES_EXPORT MLGruCellOperator : public MLOperator {
 public:
  MLGruCellOperator(MLGraphBuilder* builder,
                    uint32_t hidden_size,
                    const bindings::DictionaryBase* options);

  MLGruCellOperator(const MLGruCellOperator&) = delete;
  MLGruCellOperator& operator=(const MLGruCellOperator&) = delete;

  ~MLGruCellOperator() override;

  uint32_t hidden_size() const { return hidden_size_; }

 private:
  const uint32_t hidden_size_;
};

class MODULES_EXPORT MLPadOperator : public MLOperator {
 public:
  MLPadOperator(MLGraphBuilder* builder,
                const Vector<uint32_t>& beginning_padding,
                const Vector<uint32_t>& ending_padding,
                const bindings::DictionaryBase* options);

  MLPadOperator(const MLPadOperator&) = delete;
  MLPadOperator& operator=(const MLPadOperator&) = delete;

  ~MLPadOperator() override;

  const Vector<uint32_t>& BeginningPadding() const;
  const Vector<uint32_t>& EndingPadding() const;

 private:
  Vector<uint32_t> beginning_padding_;
  Vector<uint32_t> ending_padding_;
};

class MODULES_EXPORT MLSliceOperator : public MLOperator {
 public:
  MLSliceOperator(MLGraphBuilder* builder,
                  const Vector<uint32_t>& beginning_padding,
                  const Vector<uint32_t>& ending_padding,
                  const bindings::DictionaryBase* options);

  MLSliceOperator(const MLSliceOperator&) = delete;
  MLSliceOperator& operator=(const MLSliceOperator&) = delete;

  ~MLSliceOperator() override;

  const Vector<uint32_t>& Starts() const;
  const Vector<uint32_t>& Sizes() const;

 private:
  Vector<uint32_t> starts_;
  Vector<uint32_t> sizes_;
};

class MODULES_EXPORT MLSoftmaxOperator : public MLOperator {
 public:
  MLSoftmaxOperator(MLGraphBuilder* builder,
                    const uint32_t axis,
                    const bindings::DictionaryBase* options);

  MLSoftmaxOperator(const MLSoftmaxOperator&) = delete;
  MLSoftmaxOperator& operator=(const MLSoftmaxOperator&) = delete;

  ~MLSoftmaxOperator() override;

  uint32_t Axis() const { return axis_; }

 private:
  const uint32_t axis_;
};

class MODULES_EXPORT MLSplitOperator : public MLOperator {
 public:
  MLSplitOperator(MLGraphBuilder* builder,
                  const uint32_t splits,
                  const bindings::DictionaryBase* options);
  MLSplitOperator(MLGraphBuilder* builder,
                  const Vector<uint32_t>& splits,
                  const bindings::DictionaryBase* options);

  MLSplitOperator(const MLSplitOperator&) = delete;
  MLSplitOperator& operator=(const MLSplitOperator&) = delete;

  ~MLSplitOperator() override;

  bool IsEvenSplit() const;
  uint32_t SplitNumber() const;
  const Vector<uint32_t>& SplitSizes() const;

 private:
  bool is_even_split_;
  uint32_t split_number_;
  Vector<uint32_t> split_sizes_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
