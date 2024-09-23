// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_

#include <stddef.h>
#include <string.h>  // For |memcpy()|.

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {

class Message;

namespace internal {

template <typename Traits,
          typename MaybeConstUserType,
          bool HasGetBegin =
              HasGetBeginMethod<Traits, MaybeConstUserType>::value>
class ArrayIterator {};

// Used as the UserTypeIterator template parameter of ArraySerializer.
template <typename Traits, typename MaybeConstUserType>
class ArrayIterator<Traits, MaybeConstUserType, true> {
 public:
  using IteratorType =
      decltype(Traits::GetBegin(std::declval<MaybeConstUserType&>()));

  explicit ArrayIterator(MaybeConstUserType& input)
      : input_(input), iter_(Traits::GetBegin(input)) {}
  ~ArrayIterator() {}

  size_t GetSize() const { return Traits::GetSize(input_); }

  decltype(auto) GetNext() {
    decltype(auto) value = Traits::GetValue(iter_);
    Traits::AdvanceIterator(iter_);
    return value;
  }

  const MaybeConstUserType& input() const { return input_; }

 private:
  // RAW_PTR_EXCLUSION: Binary size increase.
  RAW_PTR_EXCLUSION MaybeConstUserType& input_;
  IteratorType iter_;
};

// Used as the UserTypeIterator template parameter of ArraySerializer.
template <typename Traits, typename MaybeConstUserType>
class ArrayIterator<Traits, MaybeConstUserType, false> {
 public:
  explicit ArrayIterator(MaybeConstUserType& input) : input_(input), iter_(0) {}
  ~ArrayIterator() {}

  size_t GetSize() const { return Traits::GetSize(input_); }

  decltype(auto) GetNext() {
    DCHECK_LT(iter_, Traits::GetSize(input_));
    return Traits::GetAt(input_, iter_++);
  }

  const MaybeConstUserType& input() const { return input_; }

 private:
  // RAW_PTR_EXCLUSION: Binary size increase.
  RAW_PTR_EXCLUSION MaybeConstUserType& input_;
  size_t iter_;
};

// ArraySerializer is also used to serialize map keys and values. Therefore, it
// has a UserTypeIterator parameter which is an adaptor for reading to hide the
// difference between ArrayTraits and MapTraits.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator,
          typename EnableType = void>
struct ArraySerializer;

// Handles serialization and deserialization of arrays of pod types.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
struct ArraySerializer<
    MojomType,
    MaybeConstUserType,
    UserTypeIterator,
    std::enable_if_t<BelongsTo<typename MojomType::Element,
                               MojomTypeCategory::kPOD>::value>> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(std::is_same<Element, DataElement>::value,
                "Incorrect array serializer");
  static_assert(
      std::is_same<
          Element,
          typename std::remove_const<typename Traits::Element>::type>::value,
      "Incorrect array serializer");

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    size_t size = input->GetSize();
    if (size == 0)
      return;

    Data* output = fragment.data();
    if constexpr (HasGetDataMethod<Traits, MaybeConstUserType>::value) {
      auto data = Traits::GetData(input->input());
      memcpy(output->storage(), data, size * sizeof(DataElement));
    } else {
      for (size_t i = 0; i < size; ++i)
        output->at(i) = input->GetNext();
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    if (input->size()) {
      if constexpr (HasGetDataMethod<Traits, UserType>::value) {
        auto data = Traits::GetData(*output);
        memcpy(data, input->storage(), input->size() * sizeof(DataElement));
      } else {
        ArrayIterator<Traits, UserType> iterator(*output);
        for (size_t i = 0; i < input->size(); ++i)
          iterator.GetNext() = static_cast<DataElement>(input->at(i));
      }
    }
    return true;
  }
};

// Handles serialization and deserialization of arrays of enum types.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
  requires(!base::is_instantiation<std::optional, typename MojomType::Element>)
struct ArraySerializer<
    MojomType,
    MaybeConstUserType,
    UserTypeIterator,
    typename std::enable_if<BelongsTo<typename MojomType::Element,
                                      MojomTypeCategory::kEnum>::value>::type> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(sizeof(Element) == sizeof(DataElement),
                "Incorrect array serializer");

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i)
      Serialize<Element>(input->GetNext(), output->storage() + i);
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      if (!Deserialize<Element>(input->at(i), &iterator.GetNext()))
        return false;
    }
    return true;
  }
};

// Handles serialization and deserialization of arrays of optional enum types.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
  requires(base::is_instantiation<std::optional, typename MojomType::Element>)
struct ArraySerializer<
    MojomType,
    MaybeConstUserType,
    UserTypeIterator,
    std::enable_if_t<BelongsTo<typename MojomType::Element,
                               MojomTypeCategory::kEnum>::value>> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(IsAbslOptional<typename Traits::Element>::value,
                "Output type should be optional");
  static_assert(sizeof(Element) == sizeof(DataElement),
                "Incorrect array serializer");

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      auto next = input->GetNext();
      if (next) {
        int32_t serialized;
        Serialize<typename Element::value_type>(*next, &serialized);
        output->at(i) = serialized;
      } else {
        output->at(i) = std::nullopt;
      }
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size())) {
      return false;
    }
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      std::optional<int32_t> element = input->at(i).ToOptional();
      if (element) {
        typename Element::value_type deserialized;
        if (!Deserialize<typename Element::value_type>(*element,
                                                       &deserialized)) {
          return false;
        }
        iterator.GetNext() = deserialized;
      } else {
        iterator.GetNext() = std::nullopt;
      }
    }
    return true;
  }
};

// Serializes and deserializes arrays of bools.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeIterator,
                       typename std::enable_if<BelongsTo<
                           typename MojomType::Element,
                           MojomTypeCategory::kBoolean>::value>::type> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = ArrayTraits<UserType>;
  using Data = typename MojomTypeTraits<MojomType>::Data;

  static_assert(std::is_same<bool, typename Traits::Element>::value,
                "Incorrect array serializer");

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i)
      output->at(i) = input->GetNext();
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i)
      iterator.GetNext() = input->at(i);
    return true;
  }
};

// Serializes and deserializes arrays of handles or interfaces.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
struct ArraySerializer<
    MojomType,
    MaybeConstUserType,
    UserTypeIterator,
    typename std::enable_if<BelongsTo<
        typename MojomType::Element,
        MojomTypeCategory::kAssociatedInterface |
            MojomTypeCategory::kAssociatedInterfaceRequest |
            MojomTypeCategory::kHandle | MojomTypeCategory::kInterface |
            MojomTypeCategory::kInterfaceRequest>::value>::type> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    DCHECK(!validate_params->element_validate_params)
        << "Handle or interface type should not have array validate params";

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      decltype(auto) next = input->GetNext();
      Serialize<Element>(next, &output->at(i), &fragment.message());

      static const ValidationError kError =
          BelongsTo<Element,
                    MojomTypeCategory::kAssociatedInterface |
                        MojomTypeCategory::kAssociatedInterfaceRequest>::value
              ? VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID
              : VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE;
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable &&
              !IsHandleOrInterfaceValid(output->at(i)),
          kError,
          MakeMessageWithArrayIndex("invalid handle or interface ID in array "
                                    "expecting valid handles or interface IDs",
                                    size, i));
    }
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      bool result =
          Deserialize<Element>(&input->at(i), &iterator.GetNext(), message);
      DCHECK(result);
    }
    return true;
  }
};

// This template must only apply to pointer mojo entity (strings, structs,
// arrays and maps).
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeIterator,
                       typename std::enable_if<BelongsTo<
                           typename MojomType::Element,
                           MojomTypeCategory::kArray | MojomTypeCategory::kMap |
                               MojomTypeCategory::kString |
                               MojomTypeCategory::kStruct>::value>::type> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using Element = typename MojomType::Element;
  using ElementData = typename MojomTypeTraits<Element>::Data;
  using Traits = ArrayTraits<UserType>;

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      MessageFragment<ElementData> data_fragment(fragment.message());
      decltype(auto) next = input->GetNext();
      SerializeCaller<Element>::Run(next, data_fragment,
                                    validate_params->element_validate_params);
      fragment->at(i).Set(data_fragment.is_null() ? nullptr
                                                  : data_fragment.data());
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && data_fragment.is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    size, i));
    }
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      if (!Deserialize<Element>(input->at(i).Get(), &iterator.GetNext(),
                                message))
        return false;
    }
    return true;
  }

 private:
  template <typename T,
            bool is_array_or_map = BelongsTo<
                T,
                MojomTypeCategory::kArray | MojomTypeCategory::kMap>::value>
  struct SerializeCaller {
    template <typename InputElementType>
    static void Run(InputElementType&& input,
                    MessageFragment<ElementData>& fragment,
                    const ContainerValidateParams* validate_params) {
      Serialize<T>(std::forward<InputElementType>(input), fragment);
    }
  };

  template <typename T>
  struct SerializeCaller<T, true> {
    template <typename InputElementType>
    static void Run(InputElementType&& input,
                    MessageFragment<ElementData>& fragment,
                    const ContainerValidateParams* validate_params) {
      Serialize<T>(std::forward<InputElementType>(input), fragment,
                   validate_params);
    }
  };
};

// Handles serialization and deserialization of arrays of unions.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
struct ArraySerializer<MojomType,
                       MaybeConstUserType,
                       UserTypeIterator,
                       typename std::enable_if<
                           BelongsTo<typename MojomType::Element,
                                     MojomTypeCategory::kUnion>::value>::type> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using Element = typename MojomType::Element;
  using DataElement = typename Data::Element;
  using Traits = ArrayTraits<UserType>;

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      MessageFragment<DataElement> inlined_union_element(fragment.message());
      inlined_union_element.Claim(fragment->storage() + i);
      decltype(auto) next = input->GetNext();
      Serialize<Element>(next, inlined_union_element, true);
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable &&
              inlined_union_element.is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    size, i));
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  Message* message) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      if (!Deserialize<Element>(&input->at(i), &iterator.GetNext(), message))
        return false;
    }
    return true;
  }
};

template <typename Element, typename MaybeConstUserType>
struct Serializer<ArrayDataView<Element>, MaybeConstUserType> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = ArrayTraits<UserType>;
  using Impl = ArraySerializer<ArrayDataView<Element>,
                               MaybeConstUserType,
                               ArrayIterator<Traits, MaybeConstUserType>>;
  using Data = typename MojomTypeTraits<ArrayDataView<Element>>::Data;

  static void Serialize(MaybeConstUserType& input,
                        MessageFragment<Data>& fragment,
                        const ContainerValidateParams* validate_params) {
    if (CallIsNullIfExists<Traits>(input))
      return;

    const size_t size = Traits::GetSize(input);
    MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
        validate_params->expected_num_elements != 0 &&
            size != validate_params->expected_num_elements,
        internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
        internal::MakeMessageWithExpectedArraySize(
            "fixed-size array has wrong number of elements", size,
            validate_params->expected_num_elements));
    fragment.AllocateArrayData(size);
    ArrayIterator<Traits, MaybeConstUserType> iterator(input);
    Impl::SerializeElements(&iterator, fragment, validate_params);
  }

  static bool Deserialize(Data* input, UserType* output, Message* message) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);
    return Impl::DeserializeElements(input, output, message);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
