// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  var validationError = {
    NONE: 'VALIDATION_ERROR_NONE',
    MISALIGNED_OBJECT: 'VALIDATION_ERROR_MISALIGNED_OBJECT',
    ILLEGAL_MEMORY_RANGE: 'VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE',
    UNEXPECTED_STRUCT_HEADER: 'VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER',
    UNEXPECTED_ARRAY_HEADER: 'VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER',
    ILLEGAL_HANDLE: 'VALIDATION_ERROR_ILLEGAL_HANDLE',
    UNEXPECTED_INVALID_HANDLE: 'VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE',
    ILLEGAL_POINTER: 'VALIDATION_ERROR_ILLEGAL_POINTER',
    UNEXPECTED_NULL_POINTER: 'VALIDATION_ERROR_UNEXPECTED_NULL_POINTER',
    ILLEGAL_INTERFACE_ID: 'VALIDATION_ERROR_ILLEGAL_INTERFACE_ID',
    UNEXPECTED_INVALID_INTERFACE_ID:
        'VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID',
    MESSAGE_HEADER_INVALID_FLAGS:
        'VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS',
    MESSAGE_HEADER_MISSING_REQUEST_ID:
        'VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID',
    DIFFERENT_SIZED_ARRAYS_IN_MAP:
        'VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP',
    INVALID_UNION_SIZE: 'VALIDATION_ERROR_INVALID_UNION_SIZE',
    UNEXPECTED_NULL_UNION: 'VALIDATION_ERROR_UNEXPECTED_NULL_UNION',
    UNKNOWN_ENUM_VALUE: 'VALIDATION_ERROR_UNKNOWN_ENUM_VALUE',
  };

  var NULL_MOJO_POINTER = "NULL_MOJO_POINTER";
  var gValidationErrorObserver = null;

  function reportValidationError(error) {
    if (gValidationErrorObserver) {
      gValidationErrorObserver.lastError = error;
    } else {
      console.warn('Invalid message: ' + error);
    }
  }

  var ValidationErrorObserverForTesting = (function() {
    function Observer() {
      this.lastError = validationError.NONE;
    }

    Observer.prototype.reset = function() {
      this.lastError = validationError.NONE;
    };

    return {
      getInstance: function() {
        if (!gValidationErrorObserver) {
          gValidationErrorObserver = new Observer();
        }
        return gValidationErrorObserver;
      }
    };
  })();

  function isTestingMode() {
    return Boolean(gValidationErrorObserver);
  }

  function clearTestingMode() {
    gValidationErrorObserver = null;
  }

  function isEnumClass(cls) {
    return cls instanceof internal.Enum;
  }

  function isStringClass(cls) {
    return cls === internal.String || cls === internal.NullableString;
  }

  function isHandleClass(cls) {
    return cls === internal.Handle || cls === internal.NullableHandle;
  }

  function isInterfaceClass(cls) {
    return cls instanceof internal.Interface;
  }

  function isInterfaceRequestClass(cls) {
    return cls === internal.InterfaceRequest ||
        cls === internal.NullableInterfaceRequest;
  }

  function isAssociatedInterfaceClass(cls) {
    return cls === internal.AssociatedInterfacePtrInfo ||
        cls === internal.NullableAssociatedInterfacePtrInfo;
  }

  function isAssociatedInterfaceRequestClass(cls) {
    return cls === internal.AssociatedInterfaceRequest ||
        cls === internal.NullableAssociatedInterfaceRequest;
  }

  function isNullable(type) {
    return type === internal.NullableString ||
        type === internal.NullableHandle ||
        type === internal.NullableAssociatedInterfacePtrInfo ||
        type === internal.NullableAssociatedInterfaceRequest ||
        type === internal.NullableInterface ||
        type === internal.NullableInterfaceRequest ||
        type instanceof internal.NullableArrayOf ||
        type instanceof internal.NullablePointerTo;
  }

  function Validator(message) {
    this.message = message;
    this.offset = 0;
    this.handleIndex = 0;
    this.associatedEndpointHandleIndex = 0;
    this.payloadInterfaceIds = null;
    this.offsetLimit = this.message.buffer.byteLength;
  }

  Object.defineProperty(Validator.prototype, "handleIndexLimit", {
    get: function() { return this.message.handles.length; }
  });

  Object.defineProperty(Validator.prototype, "associatedHandleIndexLimit", {
    get: function() {
      return this.payloadInterfaceIds ? this.payloadInterfaceIds.length : 0;
    }
  });

  // True if we can safely allocate a block of bytes from start to
  // to start + numBytes.
  Validator.prototype.isValidRange = function(start, numBytes) {
    // Only positive JavaScript integers that are less than 2^53
    // (Number.MAX_SAFE_INTEGER) can be represented exactly.
    if (start < this.offset || numBytes <= 0 ||
        !Number.isSafeInteger(start) ||
        !Number.isSafeInteger(numBytes))
      return false;

    var newOffset = start + numBytes;
    if (!Number.isSafeInteger(newOffset) || newOffset > this.offsetLimit)
      return false;

    return true;
  };

  Validator.prototype.claimRange = function(start, numBytes) {
    if (this.isValidRange(start, numBytes)) {
      this.offset = start + numBytes;
      return true;
    }
    return false;
  };

  Validator.prototype.claimHandle = function(index) {
    if (index === internal.kEncodedInvalidHandleValue)
      return true;

    if (index < this.handleIndex || index >= this.handleIndexLimit)
      return false;

    // This is safe because handle indices are uint32.
    this.handleIndex = index + 1;
    return true;
  };

  Validator.prototype.claimAssociatedEndpointHandle = function(index) {
    if (index === internal.kEncodedInvalidHandleValue) {
      return true;
    }

    if (index < this.associatedEndpointHandleIndex ||
        index >= this.associatedHandleIndexLimit) {
      return false;
    }

    // This is safe because handle indices are uint32.
    this.associatedEndpointHandleIndex = index + 1;
    return true;
  };

  Validator.prototype.validateEnum = function(offset, enumClass) {
    // Note: Assumes that enums are always 32 bits! But this matches
    // mojom::generate::pack::PackedField::GetSizeForKind, so it should be okay.
    var value = this.message.buffer.getInt32(offset);
    return enumClass.validate(value);
  }

  Validator.prototype.validateHandle = function(offset, nullable) {
    var index = this.message.buffer.getUint32(offset);

    if (index === internal.kEncodedInvalidHandleValue)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_INVALID_HANDLE;

    if (!this.claimHandle(index))
      return validationError.ILLEGAL_HANDLE;

    return validationError.NONE;
  };

  Validator.prototype.validateAssociatedEndpointHandle = function(offset,
      nullable) {
    var index = this.message.buffer.getUint32(offset);

    if (index === internal.kEncodedInvalidHandleValue) {
      return nullable ? validationError.NONE :
          validationError.UNEXPECTED_INVALID_INTERFACE_ID;
    }

    if (!this.claimAssociatedEndpointHandle(index)) {
      return validationError.ILLEGAL_INTERFACE_ID;
    }

    return validationError.NONE;
  };

  Validator.prototype.validateInterface = function(offset, nullable) {
    return this.validateHandle(offset, nullable);
  };

  Validator.prototype.validateInterfaceRequest = function(offset, nullable) {
    return this.validateHandle(offset, nullable);
  };

  Validator.prototype.validateAssociatedInterface = function(offset,
      nullable) {
    return this.validateAssociatedEndpointHandle(offset, nullable);
  };

  Validator.prototype.validateAssociatedInterfaceRequest = function(
      offset, nullable) {
    return this.validateAssociatedEndpointHandle(offset, nullable);
  };

  Validator.prototype.validateStructHeader = function(offset, minNumBytes) {
    if (!internal.isAligned(offset))
      return validationError.MISALIGNED_OBJECT;

    if (!this.isValidRange(offset, internal.kStructHeaderSize))
      return validationError.ILLEGAL_MEMORY_RANGE;

    var numBytes = this.message.buffer.getUint32(offset);

    if (numBytes < minNumBytes)
      return validationError.UNEXPECTED_STRUCT_HEADER;

    if (!this.claimRange(offset, numBytes))
      return validationError.ILLEGAL_MEMORY_RANGE;

    return validationError.NONE;
  };

  Validator.prototype.validateStructVersion = function(offset, versionSizes) {
    var numBytes = this.message.buffer.getUint32(offset);
    var version = this.message.buffer.getUint32(offset + 4);

    if (version <= versionSizes[versionSizes.length - 1].version) {
      // Scan in reverse order to optimize for more recent versionSizes.
      for (var i = versionSizes.length - 1; i >= 0; --i) {
        if (version >= versionSizes[i].version) {
          if (numBytes == versionSizes[i].numBytes)
            break;
          return validationError.UNEXPECTED_STRUCT_HEADER;
        }
      }
    } else if (numBytes < versionSizes[versionSizes.length-1].numBytes) {
      return validationError.UNEXPECTED_STRUCT_HEADER;
    }

    return validationError.NONE;
  };

  Validator.prototype.isFieldInStructVersion = function(offset, fieldVersion) {
    var structVersion = this.message.buffer.getUint32(offset + 4);
    return fieldVersion <= structVersion;
  };

  Validator.prototype.validateMessageHeader = function() {
    var err = this.validateStructHeader(0, internal.kMessageV0HeaderSize);
    if (err != validationError.NONE) {
      return err;
    }

    var numBytes = this.message.getHeaderNumBytes();
    var version = this.message.getHeaderVersion();

    var validVersionAndNumBytes =
        (version == 0 && numBytes == internal.kMessageV0HeaderSize) ||
        (version == 1 && numBytes == internal.kMessageV1HeaderSize) ||
        (version == 2 && numBytes == internal.kMessageV2HeaderSize) ||
        (version > 2 && numBytes >= internal.kMessageV2HeaderSize);

    if (!validVersionAndNumBytes) {
      return validationError.UNEXPECTED_STRUCT_HEADER;
    }

    var expectsResponse = this.message.expectsResponse();
    var isResponse = this.message.isResponse();

    if (version == 0 && (expectsResponse || isResponse)) {
      return validationError.MESSAGE_HEADER_MISSING_REQUEST_ID;
    }

    if (isResponse && expectsResponse) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }

    if (version < 2) {
      return validationError.NONE;
    }

    var err = this.validateArrayPointer(
        internal.kMessagePayloadInterfaceIdsPointerOffset,
        internal.Uint32.encodedSize, internal.Uint32, true, [0], 0);

    if (err != validationError.NONE) {
      return err;
    }

    this.payloadInterfaceIds = this.message.getPayloadInterfaceIds();
    if (this.payloadInterfaceIds) {
      for (var interfaceId of this.payloadInterfaceIds) {
        if (!internal.isValidInterfaceId(interfaceId) ||
            internal.isPrimaryInterfaceId(interfaceId)) {
          return validationError.ILLEGAL_INTERFACE_ID;
        }
      }
    }

    // Set offset to the start of the payload and offsetLimit to the start of
    // the payload interface Ids so that payload can be validated using the
    // same messageValidator.
    this.offset = this.message.getHeaderNumBytes();
    this.offsetLimit = this.decodePointer(
        internal.kMessagePayloadInterfaceIdsPointerOffset);

    return validationError.NONE;
  };

  Validator.prototype.validateMessageIsRequestWithoutResponse = function() {
    if (this.message.isResponse() || this.message.expectsResponse()) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateMessageIsRequestExpectingResponse = function() {
    if (this.message.isResponse() || !this.message.expectsResponse()) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateMessageIsResponse = function() {
    if (this.message.expectsResponse() || !this.message.isResponse()) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }
    return validationError.NONE;
  };

  // Returns the message.buffer relative offset this pointer "points to",
  // NULL_MOJO_POINTER if the pointer represents a null, or JS null if the
  // pointer's value is not valid.
  Validator.prototype.decodePointer = function(offset) {
    var pointerValue = this.message.buffer.getUint64(offset);
    if (pointerValue === 0)
      return NULL_MOJO_POINTER;
    var bufferOffset = offset + pointerValue;
    return Number.isSafeInteger(bufferOffset) ? bufferOffset : null;
  };

  Validator.prototype.decodeUnionSize = function(offset) {
    return this.message.buffer.getUint32(offset);
  };

  Validator.prototype.decodeUnionTag = function(offset) {
    return this.message.buffer.getUint32(offset + 4);
  };

  Validator.prototype.validateArrayPointer = function(
      offset, elementSize, elementType, nullable, expectedDimensionSizes,
      currentDimension) {
    var arrayOffset = this.decodePointer(offset);
    if (arrayOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (arrayOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    return this.validateArray(arrayOffset, elementSize, elementType,
                              expectedDimensionSizes, currentDimension);
  };

  Validator.prototype.validateStructPointer = function(
      offset, structClass, nullable) {
    var structOffset = this.decodePointer(offset);
    if (structOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (structOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    return structClass.validate(this, structOffset);
  };

  Validator.prototype.validateUnion = function(
      offset, unionClass, nullable) {
    var size = this.message.buffer.getUint32(offset);
    if (size == 0) {
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_UNION;
    }

    return unionClass.validate(this, offset);
  };

  Validator.prototype.validateNestedUnion = function(
      offset, unionClass, nullable) {
    var unionOffset = this.decodePointer(offset);
    if (unionOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (unionOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_UNION;

    return this.validateUnion(unionOffset, unionClass, nullable);
  };

  // This method assumes that the array at arrayPointerOffset has
  // been validated.

  Validator.prototype.arrayLength = function(arrayPointerOffset) {
    var arrayOffset = this.decodePointer(arrayPointerOffset);
    return this.message.buffer.getUint32(arrayOffset + 4);
  };

  Validator.prototype.validateMapPointer = function(
      offset, mapIsNullable, keyClass, valueClass, valueIsNullable) {
    // Validate the implicit map struct:
    // struct {array<keyClass> keys; array<valueClass> values};
    var structOffset = this.decodePointer(offset);
    if (structOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (structOffset === NULL_MOJO_POINTER)
      return mapIsNullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    var mapEncodedSize = internal.kStructHeaderSize +
                         internal.kMapStructPayloadSize;
    var err = this.validateStructHeader(structOffset, mapEncodedSize);
    if (err !== validationError.NONE)
        return err;

    // Validate the keys array.
    var keysArrayPointerOffset = structOffset + internal.kStructHeaderSize;
    err = this.validateArrayPointer(
        keysArrayPointerOffset, keyClass.encodedSize, keyClass, false, [0], 0);
    if (err !== validationError.NONE)
        return err;

    // Validate the values array.
    var valuesArrayPointerOffset = keysArrayPointerOffset + 8;
    var valuesArrayDimensions = [0]; // Validate the actual length below.
    if (valueClass instanceof internal.ArrayOf)
      valuesArrayDimensions =
          valuesArrayDimensions.concat(valueClass.dimensions());
    var err = this.validateArrayPointer(valuesArrayPointerOffset,
                                        valueClass.encodedSize,
                                        valueClass,
                                        valueIsNullable,
                                        valuesArrayDimensions,
                                        0);
    if (err !== validationError.NONE)
        return err;

    // Validate the lengths of the keys and values arrays.
    var keysArrayLength = this.arrayLength(keysArrayPointerOffset);
    var valuesArrayLength = this.arrayLength(valuesArrayPointerOffset);
    if (keysArrayLength != valuesArrayLength)
      return validationError.DIFFERENT_SIZED_ARRAYS_IN_MAP;

    return validationError.NONE;
  };

  Validator.prototype.validateStringPointer = function(offset, nullable) {
    return this.validateArrayPointer(
        offset, internal.Uint8.encodedSize, internal.Uint8, nullable, [0], 0);
  };

  // Similar to Array_Data<T>::Validate()
  // mojo/public/cpp/bindings/lib/array_internal.h

  Validator.prototype.validateArray =
      function (offset, elementSize, elementType, expectedDimensionSizes,
                currentDimension) {
    if (!internal.isAligned(offset))
      return validationError.MISALIGNED_OBJECT;

    if (!this.isValidRange(offset, internal.kArrayHeaderSize))
      return validationError.ILLEGAL_MEMORY_RANGE;

    var numBytes = this.message.buffer.getUint32(offset);
    var numElements = this.message.buffer.getUint32(offset + 4);

    // Note: this computation is "safe" because elementSize <= 8 and
    // numElements is a uint32.
    var elementsTotalSize = (elementType === internal.PackedBool) ?
        Math.ceil(numElements / 8) : (elementSize * numElements);

    if (numBytes < internal.kArrayHeaderSize + elementsTotalSize)
      return validationError.UNEXPECTED_ARRAY_HEADER;

    if (expectedDimensionSizes[currentDimension] != 0 &&
        numElements != expectedDimensionSizes[currentDimension]) {
      return validationError.UNEXPECTED_ARRAY_HEADER;
    }

    if (!this.claimRange(offset, numBytes))
      return validationError.ILLEGAL_MEMORY_RANGE;

    // Validate the array's elements if they are pointers or handles.

    var elementsOffset = offset + internal.kArrayHeaderSize;
    var nullable = isNullable(elementType);

    if (isHandleClass(elementType))
      return this.validateHandleElements(elementsOffset, numElements, nullable);
    if (isInterfaceClass(elementType))
      return this.validateInterfaceElements(
          elementsOffset, numElements, nullable);
    if (isInterfaceRequestClass(elementType))
      return this.validateInterfaceRequestElements(
          elementsOffset, numElements, nullable);
    if (isAssociatedInterfaceClass(elementType))
      return this.validateAssociatedInterfaceElements(
          elementsOffset, numElements, nullable);
    if (isAssociatedInterfaceRequestClass(elementType))
      return this.validateAssociatedInterfaceRequestElements(
          elementsOffset, numElements, nullable);
    if (isStringClass(elementType))
      return this.validateArrayElements(
          elementsOffset, numElements, internal.Uint8, nullable, [0], 0);
    if (elementType instanceof internal.PointerTo)
      return this.validateStructElements(
          elementsOffset, numElements, elementType.cls, nullable);
    if (elementType instanceof internal.ArrayOf)
      return this.validateArrayElements(
          elementsOffset, numElements, elementType.cls, nullable,
          expectedDimensionSizes, currentDimension + 1);
    if (isEnumClass(elementType))
      return this.validateEnumElements(elementsOffset, numElements,
                                       elementType.cls);

    return validationError.NONE;
  };

  // Note: the |offset + i * elementSize| computation in the validateFooElements
  // methods below is "safe" because elementSize <= 8, offset and
  // numElements are uint32, and 0 <= i < numElements.

  Validator.prototype.validateHandleElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.Handle.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateHandle(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateInterfaceElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.Interface.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateInterface(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateInterfaceRequestElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.InterfaceRequest.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateInterfaceRequest(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateAssociatedInterfaceElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.AssociatedInterfacePtrInfo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateAssociatedInterface(elementOffset, nullable);
      if (err != validationError.NONE) {
        return err;
      }
    }
    return validationError.NONE;
  };

  Validator.prototype.validateAssociatedInterfaceRequestElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.AssociatedInterfaceRequest.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateAssociatedInterfaceRequest(elementOffset,
          nullable);
      if (err != validationError.NONE) {
        return err;
      }
    }
    return validationError.NONE;
  };

  // The elementClass parameter is the element type of the element arrays.
  Validator.prototype.validateArrayElements =
      function(offset, numElements, elementClass, nullable,
               expectedDimensionSizes, currentDimension) {
    var elementSize = internal.PointerTo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateArrayPointer(
          elementOffset, elementClass.encodedSize, elementClass, nullable,
          expectedDimensionSizes, currentDimension);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateStructElements =
      function(offset, numElements, structClass, nullable) {
    var elementSize = internal.PointerTo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err =
          this.validateStructPointer(elementOffset, structClass, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateEnumElements =
      function(offset, numElements, enumClass) {
    var elementSize = internal.Enum.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateEnum(elementOffset, enumClass);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  internal.validationError = validationError;
  internal.Validator = Validator;
  internal.ValidationErrorObserverForTesting =
      ValidationErrorObserverForTesting;
  internal.reportValidationError = reportValidationError;
  internal.isTestingMode = isTestingMode;
  internal.clearTestingMode = clearTestingMode;
})();
