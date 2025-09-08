// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_SEND_VALIDATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_SEND_VALIDATION_H_

#include <type_traits>

#include "mojo/public/cpp/bindings/array_data_view.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/send_validation.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo::internal {

// ArraySerializer is also used to serialize map keys and values. Therefore, it
// has a UserTypeIterator parameter which is an adaptor for reading to hide the
// difference between ArrayTraits and MapTraits.
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator,
          typename EnableType = void>
struct SendValidationArraySerializer;

// Helper to detect if a specialization of SendValidationArraySerializer exists
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator,
          typename = void>
struct HasSendValidationArraySerializer : std::false_type {};

template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
struct HasSendValidationArraySerializer<
    MojomType,
    MaybeConstUserType,
    send_validation,
    UserTypeIterator,
    std::void_t<decltype(SendValidationArraySerializer<MojomType,
                                                       MaybeConstUserType,
                                                       send_validation,
                                                       UserTypeIterator>{})>>
    : std::true_type {};

template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
using SelectArraySerializer = std::conditional_t<
    HasSendValidationArraySerializer<MojomType,
                                     MaybeConstUserType,
                                     send_validation,
                                     UserTypeIterator>::value,
    SendValidationArraySerializer<MojomType,
                                  MaybeConstUserType,
                                  send_validation,
                                  UserTypeIterator>,
    ArraySerializer<MojomType, MaybeConstUserType, UserTypeIterator>>;

// Handles serialization and deserialization of arrays of enum types.
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
  requires(!base::is_instantiation<typename MojomType::Element, std::optional>)
struct SendValidationArraySerializer<
    MojomType,
    MaybeConstUserType,
    send_validation,
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
    if constexpr (send_validation == SendValidation::kFatal) {
      CHECK(!validate_params->element_is_nullable)
          << "Primitive type should be non-nullable";
      CHECK(!validate_params->element_validate_params)
          << "Primitive type should not have array validate params";
    } else if constexpr (send_validation == SendValidation::kWarning) {
      DCHECK(!validate_params->element_is_nullable)
          << "Primitive type should be non-nullable";
      DCHECK(!validate_params->element_validate_params)
          << "Primitive type should not have array validate params";
    }

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      Serialize<Element, send_validation>(input->GetNext(),
                                          output->storage() + i);
    }
  }
};

// Handles serialization and deserialization of arrays of optional enum types.
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
  requires(base::is_instantiation<typename MojomType::Element, std::optional>)
struct SendValidationArraySerializer<
    MojomType,
    MaybeConstUserType,
    send_validation,
    UserTypeIterator,
    std::enable_if_t<BelongsTo<typename MojomType::Element,
                               MojomTypeCategory::kEnum>::value>> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Data = typename MojomTypeTraits<MojomType>::Data;
  using DataElement = typename Data::Element;
  using Element = typename MojomType::Element;
  using Traits = ArrayTraits<UserType>;

  static_assert(IsStdOptional<typename Traits::Element>::value,
                "Output type should be optional");
  static_assert(sizeof(Element) == sizeof(DataElement),
                "Incorrect array serializer");

  static void SerializeElements(
      UserTypeIterator* input,
      MessageFragment<Data>& fragment,
      const ContainerValidateParams* validate_params) {
    if constexpr (send_validation == SendValidation::kFatal) {
      CHECK(!validate_params->element_validate_params)
          << "Primitive type should not have array validate params";
    } else if constexpr (send_validation == SendValidation::kWarning) {
      DCHECK(!validate_params->element_validate_params)
          << "Primitive type should not have array validate params";
    }

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      auto next = input->GetNext();
      if (next) {
        int32_t serialized;
        Serialize<typename Element::value_type, send_validation>(*next,
                                                                 &serialized);
        output->at(i) = serialized;
      } else {
        output->at(i) = std::nullopt;
      }
    }
  }
};

// Serializes and deserializes arrays of handles or interfaces.
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
struct SendValidationArraySerializer<
    MojomType,
    MaybeConstUserType,
    send_validation,
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
    if constexpr (send_validation == SendValidation::kFatal) {
      CHECK(!validate_params->element_validate_params)
          << "Handle or interface type should not have array validate params";
    } else if constexpr (send_validation == SendValidation::kWarning) {
      DCHECK(!validate_params->element_validate_params)
          << "Handle or interface type should not have array validate params";
    }

    Data* output = fragment.data();
    size_t size = input->GetSize();
    for (size_t i = 0; i < size; ++i) {
      decltype(auto) next = input->GetNext();
      Serialize<Element, send_validation>(next, &output->at(i),
                                          &fragment.message());

      static const ValidationError kError =
          BelongsTo<Element,
                    MojomTypeCategory::kAssociatedInterface |
                        MojomTypeCategory::kAssociatedInterfaceRequest>::value
              ? VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID
              : VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE;

      MOJO_INTERNAL_CHECK_SERIALIZATION(
          send_validation,
          !(!validate_params->element_is_nullable &&
            !IsHandleOrInterfaceValid(output->at(i))),
          kError,
          MakeMessageWithArrayIndex("invalid handle or interface ID in array "
                                    "expecting valid handles or interface IDs",
                                    size, i));
    }
  }
};

// This template must only apply to pointer mojo entity (strings, structs,
// arrays and maps).
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
struct SendValidationArraySerializer<
    MojomType,
    MaybeConstUserType,
    send_validation,
    UserTypeIterator,
    typename std::enable_if<
        BelongsTo<typename MojomType::Element,
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

      MOJO_INTERNAL_CHECK_SERIALIZATION(
          send_validation,
          !(!validate_params->element_is_nullable && data_fragment.is_null()),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid pointers",
                                    size, i));
    }
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
      Serialize<T, send_validation>(std::forward<InputElementType>(input),
                                    fragment);
    }
  };

  template <typename T>
  struct SerializeCaller<T, true> {
    template <typename InputElementType>
    static void Run(InputElementType&& input,
                    MessageFragment<ElementData>& fragment,
                    const ContainerValidateParams* validate_params) {
      Serialize<T, send_validation>(std::forward<InputElementType>(input),
                                    fragment, validate_params);
    }
  };
};

// Handles serialization and deserialization of arrays of unions.
template <typename MojomType,
          typename MaybeConstUserType,
          SendValidation send_validation,
          typename UserTypeIterator>
struct SendValidationArraySerializer<
    MojomType,
    MaybeConstUserType,
    send_validation,
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
      inlined_union_element.Claim(&fragment->at(i));
      decltype(auto) next = input->GetNext();
      Serialize<Element, send_validation>(next, inlined_union_element, true);

      MOJO_INTERNAL_CHECK_SERIALIZATION(
          send_validation,
          !(!validate_params->element_is_nullable &&
            inlined_union_element.is_null()),
          VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
          MakeMessageWithArrayIndex("null in array expecting valid unions",
                                    size, i));
    }
  }
};

template <typename Element,
          typename MaybeConstUserType,
          SendValidation send_validation>
struct SendValidationSerializer<ArrayDataView<Element>,
                                MaybeConstUserType,
                                send_validation> {
  using UserType = typename std::remove_const<MaybeConstUserType>::type;
  using Traits = ArrayTraits<UserType>;
  using Impl = SelectArraySerializer<ArrayDataView<Element>,
                                     MaybeConstUserType,
                                     send_validation,
                                     ArrayIterator<Traits, MaybeConstUserType>>;
  using Data = typename MojomTypeTraits<ArrayDataView<Element>>::Data;

  static void Serialize(MaybeConstUserType& input,
                        MessageFragment<Data>& fragment,
                        const ContainerValidateParams* validate_params) {
    if (CallIsNullIfExists<Traits>(input)) {
      return;
    }

    const size_t size = Traits::GetSize(input);

    MOJO_INTERNAL_CHECK_SERIALIZATION(
        send_validation,
        !(validate_params->expected_num_elements != 0 &&
          size != validate_params->expected_num_elements),
        VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
        MakeMessageWithExpectedArraySize(
            "fixed-size array has wrong number of elements", size,
            validate_params->expected_num_elements));

    fragment.AllocateArrayData(size);
    ArrayIterator<Traits, MaybeConstUserType> iterator(input);
    Impl::SerializeElements(&iterator, fragment, validate_params);
  }
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ARRAY_SERIALIZATION_SEND_VALIDATION_H_
