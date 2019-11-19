// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_

#include <stddef.h>
#include <string.h>  // For |memcpy()|.

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {
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
  using IteratorType = decltype(
      CallGetBeginIfExists<Traits>(std::declval<MaybeConstUserType&>()));

  explicit ArrayIterator(MaybeConstUserType& input)
      : input_(input), iter_(CallGetBeginIfExists<Traits>(input)) {}
  ~ArrayIterator() {}

  size_t GetSize() const { return Traits::GetSize(input_); }

  using GetNextResult =
      decltype(Traits::GetValue(std::declval<IteratorType&>()));
  GetNextResult GetNext() {
    GetNextResult value = Traits::GetValue(iter_);
    Traits::AdvanceIterator(iter_);
    return value;
  }

  using GetDataIfExistsResult = decltype(
      CallGetDataIfExists<Traits>(std::declval<MaybeConstUserType&>()));
  GetDataIfExistsResult GetDataIfExists() {
    return CallGetDataIfExists<Traits>(input_);
  }

 private:
  MaybeConstUserType& input_;
  IteratorType iter_;
};

// Used as the UserTypeIterator template parameter of ArraySerializer.
template <typename Traits, typename MaybeConstUserType>
class ArrayIterator<Traits, MaybeConstUserType, false> {
 public:
  explicit ArrayIterator(MaybeConstUserType& input) : input_(input), iter_(0) {}
  ~ArrayIterator() {}

  size_t GetSize() const { return Traits::GetSize(input_); }

  using GetNextResult =
      decltype(Traits::GetAt(std::declval<MaybeConstUserType&>(), 0));
  GetNextResult GetNext() {
    DCHECK_LT(iter_, Traits::GetSize(input_));
    return Traits::GetAt(input_, iter_++);
  }

  using GetDataIfExistsResult = decltype(
      CallGetDataIfExists<Traits>(std::declval<MaybeConstUserType&>()));
  GetDataIfExistsResult GetDataIfExists() {
    return CallGetDataIfExists<Traits>(input_);
  }

 private:
  MaybeConstUserType& input_;
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
    typename std::enable_if<BelongsTo<typename MojomType::Element,
                                      MojomTypeCategory::kPOD>::value>::type> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;
  using BufferWriter = typename Data::BufferWriter;

  static_assert(std::is_same<Element, DataElement>::value,
                "Incorrect array serializer");
  static_assert(
      std::is_same<
          Element,
          typename std::remove_const<typename Traits::Element>::type>::value,
      "Incorrect array serializer");

  static void SerializeElements(UserTypeIterator* input,
                                Buffer* buf,
                                BufferWriter* writer,
                                const ContainerValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    size_t size = input->GetSize();
    if (size == 0)
      return;

    auto data = input->GetDataIfExists();
    Data* output = writer->data();
    if (data) {
      memcpy(output->storage(), data, size * sizeof(DataElement));
    } else {
      for (size_t i = 0; i < size; ++i)
        output->at(i) = input->GetNext();
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    if (input->size()) {
      auto data = iterator.GetDataIfExists();
      if (data) {
        memcpy(data, input->storage(), input->size() * sizeof(DataElement));
      } else {
        for (size_t i = 0; i < input->size(); ++i)
          iterator.GetNext() = input->at(i);
      }
    }
    return true;
  }
};

// Handles serialization and deserialization of arrays of enum types.
template <typename MojomType,
          typename MaybeConstUserType,
          typename UserTypeIterator>
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
  using BufferWriter = typename Data::BufferWriter;

  static_assert(sizeof(Element) == sizeof(DataElement),
                "Incorrect array serializer");

  static void SerializeElements(UserTypeIterator* input,
                                Buffer* buf,
                                BufferWriter* writer,
                                const ContainerValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    Data* output = writer->data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i)
      Serialize<Element>(input->GetNext(), output->storage() + i);
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
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
  using BufferWriter = typename Data::BufferWriter;

  static_assert(std::is_same<bool, typename Traits::Element>::value,
                "Incorrect array serializer");

  static void SerializeElements(UserTypeIterator* input,
                                Buffer* buf,
                                BufferWriter* writer,
                                const ContainerValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_is_nullable)
        << "Primitive type should be non-nullable";
    DCHECK(!validate_params->element_validate_params)
        << "Primitive type should not have array validate params";

    Data* output = writer->data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i)
      output->at(i) = input->GetNext();
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
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
  using BufferWriter = typename Data::BufferWriter;

  static void SerializeElements(UserTypeIterator* input,
                                Buffer* buf,
                                BufferWriter* writer,
                                const ContainerValidateParams* validate_params,
                                SerializationContext* context) {
    DCHECK(!validate_params->element_validate_params)
        << "Handle or interface type should not have array validate params";

    Data* output = writer->data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      typename UserTypeIterator::GetNextResult next = input->GetNext();
      Serialize<Element>(next, &output->at(i), context);

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
                                  SerializationContext* context) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      bool result =
          Deserialize<Element>(&input->at(i), &iterator.GetNext(), context);
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
  using DataElementWriter =
      typename MojomTypeTraits<Element>::Data::BufferWriter;
  using Traits = ArrayTraits<UserType>;
  using BufferWriter = typename Data::BufferWriter;

  static void SerializeElements(UserTypeIterator* input,
                                Buffer* buf,
                                BufferWriter* writer,
                                const ContainerValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      DataElementWriter data_writer;
      typename UserTypeIterator::GetNextResult next = input->GetNext();
      SerializeCaller<Element>::Run(next, buf, &data_writer,
                                    validate_params->element_validate_params,
                                    context);
      writer->data()->at(i).Set(data_writer.is_null() ? nullptr
                                                      : data_writer.data());
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable && data_writer.is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    size, i));
    }
  }
  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      if (!Deserialize<Element>(input->at(i).Get(), &iterator.GetNext(),
                                context))
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
                    Buffer* buf,
                    DataElementWriter* writer,
                    const ContainerValidateParams* validate_params,
                    SerializationContext* context) {
      Serialize<T>(std::forward<InputElementType>(input), buf, writer, context);
    }
  };

  template <typename T>
  struct SerializeCaller<T, true> {
    template <typename InputElementType>
    static void Run(InputElementType&& input,
                    Buffer* buf,
                    DataElementWriter* writer,
                    const ContainerValidateParams* validate_params,
                    SerializationContext* context) {
      Serialize<T>(std::forward<InputElementType>(input), buf, writer,
                   validate_params, context);
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
  using ElementWriter = typename Data::Element::BufferWriter;
  using Traits = ArrayTraits<UserType>;
  using BufferWriter = typename Data::BufferWriter;

  static void SerializeElements(UserTypeIterator* input,
                                Buffer* buf,
                                BufferWriter* writer,
                                const ContainerValidateParams* validate_params,
                                SerializationContext* context) {
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      ElementWriter result;
      result.AllocateInline(buf, writer->data()->storage() + i);
      typename UserTypeIterator::GetNextResult next = input->GetNext();
      Serialize<Element>(next, buf, &result, true, context);
      MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
          !validate_params->element_is_nullable &&
              writer->data()->at(i).is_null(),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    size, i));
    }
  }

  static bool DeserializeElements(Data* input,
                                  UserType* output,
                                  SerializationContext* context) {
    if (!Traits::Resize(*output, input->size()))
      return false;
    ArrayIterator<Traits, UserType> iterator(*output);
    for (size_t i = 0; i < input->size(); ++i) {
      if (!Deserialize<Element>(&input->at(i), &iterator.GetNext(), context))
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
  using BufferWriter = typename Data::BufferWriter;

  static void Serialize(MaybeConstUserType& input,
                        Buffer* buf,
                        BufferWriter* writer,
                        const ContainerValidateParams* validate_params,
                        SerializationContext* context) {
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
    writer->Allocate(size, buf);
    ArrayIterator<Traits, MaybeConstUserType> iterator(input);
    Impl::SerializeElements(&iterator, buf, writer, validate_params, context);
  }

  static bool Deserialize(Data* input,
                          UserType* output,
                          SerializationContext* context) {
    if (!input)
      return CallSetToNullIfExists<Traits>(output);
    return Impl::DeserializeElements(input, output, context);
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_H_
