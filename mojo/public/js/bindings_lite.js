// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {number} */
mojo.internal.kArrayHeaderSize = 8;

/** @const {number} */
mojo.internal.kStructHeaderSize = 8;

/** @const {number} */
mojo.internal.kUnionHeaderSize = 8;

/** @const {number} */
mojo.internal.kUnionDataSize = 16;

/** @const {number} */
mojo.internal.kMessageV0HeaderSize = 24;

/** @const {number} */
mojo.internal.kMessageV1HeaderSize = 32;

/** @const {number} */
mojo.internal.kMessageV2HeaderSize = 48;

/** @const {number} */
mojo.internal.kMapDataSize = 24;

/** @const {number} */
mojo.internal.kEncodedInvalidHandleValue = 0xffffffff;

/** @const {number} */
mojo.internal.kMessageFlagExpectsResponse = 1 << 0;

/** @const {number} */
mojo.internal.kMessageFlagIsResponse = 1 << 1;

/** @const {number} */
mojo.internal.kInterfaceNamespaceBit = 0x80000000;

/** @const {boolean} */
mojo.internal.kHostLittleEndian = (function() {
  const wordBytes = new Uint8Array(new Uint16Array([1]).buffer);
  return !!wordBytes[0];
})();

/**
 * @param {*} x
 * @return {boolean}
 */
mojo.internal.isNullOrUndefined = function(x) {
  return x === null || x === undefined;
};

/**
 * @param {*} x
 * @return {boolean}
 */
mojo.internal.isNullableValueKindField = function(x) {
  return typeof x.nullableValueKindProperties !== 'undefined';
}

/**
 * @param {number} size
 * @param {number} alignment
 * @return {number}
 */
mojo.internal.align = function(size, alignment) {
  return size + (alignment - (size % alignment)) % alignment;
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @param {number|bigint} value
 */
mojo.internal.setInt64 = function(dataView, byteOffset, value) {
  if (mojo.internal.kHostLittleEndian) {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setInt32(
        byteOffset + 4,
        Number((BigInt(value) >> BigInt(32)) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  } else {
    dataView.setInt32(
        byteOffset, Number((BigInt(value) >> BigInt(32)) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  }
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @param {number|bigint} value
 */
mojo.internal.setUint64 = function(dataView, byteOffset, value) {
  if (mojo.internal.kHostLittleEndian) {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4,
        Number((BigInt(value) >> BigInt(32)) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  } else {
    dataView.setUint32(
        byteOffset, Number((BigInt(value) >> BigInt(32)) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  }
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @return {bigint}
 */
mojo.internal.getInt64 = function(dataView, byteOffset) {
  let low, high;
  if (mojo.internal.kHostLittleEndian) {
    low = dataView.getUint32(byteOffset, mojo.internal.kHostLittleEndian);
    high = dataView.getInt32(byteOffset + 4, mojo.internal.kHostLittleEndian);
  } else {
    low = dataView.getUint32(byteOffset + 4, mojo.internal.kHostLittleEndian);
    high = dataView.getInt32(byteOffset, mojo.internal.kHostLittleEndian);
  }
  return (BigInt(high) << BigInt(32)) | BigInt(low);
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @return {bigint}
 */
mojo.internal.getUint64 = function(dataView, byteOffset) {
  let low, high;
  if (mojo.internal.kHostLittleEndian) {
    low = dataView.getUint32(byteOffset, mojo.internal.kHostLittleEndian);
    high = dataView.getUint32(byteOffset + 4, mojo.internal.kHostLittleEndian);
  } else {
    low = dataView.getUint32(byteOffset + 4, mojo.internal.kHostLittleEndian);
    high = dataView.getUint32(byteOffset, mojo.internal.kHostLittleEndian);
  }
  return (BigInt(high) << BigInt(32)) | BigInt(low);
};

/**
 * @typedef {{
 *   size: number,
 *   numInterfaceIds: (number|undefined),
 * }}
 */
mojo.internal.MessageDimensions;

/**
 * This computes the total amount of buffer space required to hold a struct
 * value and all its fields, including indirect objects like arrays, structs,
 * and nullable unions.
 *
 * @param {!mojo.internal.StructSpec} structSpec
 * @param {!Object} value
 * @return {!mojo.internal.MessageDimensions}
 */
mojo.internal.computeStructDimensions = function(structSpec, value) {
  let size = structSpec.packedSize;
  let numInterfaceIds = 0;
  for (const field of structSpec.fields) {
    let fieldValue = value[field.name];
    if (mojo.internal.isNullOrUndefined(fieldValue)) {
      fieldValue = field.defaultValue;
    }
    if (fieldValue === null) {
      continue;
    }

    if (field.type.$.computeDimensions) {
      const fieldDimensions =
          field.type.$.computeDimensions(fieldValue, field.nullable);
      size += mojo.internal.align(fieldDimensions.size, 8);
      numInterfaceIds += fieldDimensions.numInterfaceIds;
    } else if (field.type.$.hasInterfaceId) {
      numInterfaceIds++;
    }
  }
  return {size, numInterfaceIds};
};

/**
 * @param {!mojo.internal.UnionSpec} unionSpec
 * @param {!Object} value
 * @return {!mojo.internal.MessageDimensions}
 */
mojo.internal.computeUnionDimensions = function(unionSpec, nullable, value) {
  // Unions are normally inlined since they're always a fixed width of 16
  // bytes, but nullable union-typed fields require indirection. Hence this
  // unique special case where a union field requires additional storage
  // beyond the struct's own packed field data only when it's nullable.
  let size = nullable ? mojo.internal.kUnionDataSize : 0;
  let numInterfaceIds = 0;

  const keys = Object.keys(value);
  if (keys.length !== 1) {
    throw new Error(
        `Value for ${unionSpec.name} must be an Object with a ` +
        'single property named one of: ' +
        Object.keys(unionSpec.fields).join(','));
  }

  const tag = keys[0];
  const field = unionSpec.fields[tag];
  const fieldValue = value[tag];
  if (!mojo.internal.isNullOrUndefined(fieldValue)) {
    // Nested unions are always encoded with indirection, which we induce by
    // claiming the field is nullable even if it's not.
    if (field['type'].$.computeDimensions) {
      const nullable = !!field['type'].$.unionSpec || field['nullable'];
      const fieldDimensions =
          field['type'].$.computeDimensions(fieldValue, nullable);
      size += mojo.internal.align(fieldDimensions.size, 8);
      numInterfaceIds += fieldDimensions.numInterfaceIds;
    } else if (field['type'].$.hasInterfaceId) {
      numInterfaceIds++;
    }
  }

  return {size, numInterfaceIds};
};

/**
 * @param {!mojo.internal.ArraySpec} arraySpec
 * @param {!Array|!Uint8Array} value
 * @return {number}
 */
mojo.internal.computeInlineArraySize = function(arraySpec, value) {
  if (arraySpec.elementType === mojo.internal.Bool) {
    return mojo.internal.kArrayHeaderSize +
        mojo.internal.computeHasValueBitfieldSize(arraySpec, value.length) +
        ((value.length + 7) >> 3);
  } else {
    return mojo.internal.kArrayHeaderSize +
        mojo.internal.computeHasValueBitfieldSize(arraySpec, value.length) +
        value.length *
        arraySpec.elementType.$.arrayElementSize(!!arraySpec.elementNullable);
  }
};

/**
 * @param {!mojo.internal.ArraySpec} arraySpec
 * @param {number} length
 * @return {number} the number of bytes needed for the an array's has-value
 *   bitfield. If the arraySpec does not require a has-value bitfield, this
 *   method will return 0.
 */
mojo.internal.computeHasValueBitfieldSize = function(arraySpec, length) {
    const isNullableValueType = !!arraySpec.elementNullable &&
        !!arraySpec.elementType.$.isValueType;
    if (!isNullableValueType) {
      return 0;
    }
    const element_type_bytes =
        arraySpec.elementType.$.arrayElementSize(/* nullable= */ true);
    const element_type_bits = element_type_bytes * 8;
    const needed_bits = length + element_type_bits - 1;
    // >> 0 to force integer arithmetic.
    return  ((needed_bits/element_type_bits) >> 0)  * element_type_bytes;
}

/**
 * @param {!mojo.internal.ArraySpec} arraySpec
 * @param {!Array|!Uint8Array} value
 * @return {number}
 */
mojo.internal.computeTotalArraySize = function(arraySpec, value) {
  const inlineSize = mojo.internal.computeInlineArraySize(arraySpec, value);
  if (!arraySpec.elementType.$.computeDimensions)
    return inlineSize;

  let totalSize = inlineSize;
  for (let elementValue of value) {
    if (!mojo.internal.isNullOrUndefined(elementValue)) {
      totalSize += mojo.internal.align(
          arraySpec.elementType.$
              .computeDimensions(elementValue, !!arraySpec.elementNullable)
              .size,
          8);
    }
  }

  return totalSize;
};

/** Owns an outgoing message buffer and facilitates serialization. */
mojo.internal.Message = class {
  /**
   * @param {?mojo.internal.interfaceSupport.Endpoint} sender
   * @param {number} interfaceId
   * @param {number} flags
   * @param {number} ordinal
   * @param {number} requestId
   * @param {!mojo.internal.StructSpec} paramStructSpec
   * @param {!Object} value
   * @public
   */
  constructor(
      sender, interfaceId, flags, ordinal, requestId, paramStructSpec, value) {
    const dimensions =
        mojo.internal.computeStructDimensions(paramStructSpec, value);

    let headerSize, version;
    if (dimensions.numInterfaceIds > 0) {
      headerSize = mojo.internal.kMessageV2HeaderSize;
      version = 2;
    } else if (
        (flags &
         (mojo.internal.kMessageFlagExpectsResponse |
          mojo.internal.kMessageFlagIsResponse)) == 0) {
      headerSize = mojo.internal.kMessageV0HeaderSize;
      version = 0;
    } else {
      headerSize = mojo.internal.kMessageV1HeaderSize;
      version = 1;
    }

    const headerWithPayloadSize = headerSize + dimensions.size;
    const interfaceIdsSize = dimensions.numInterfaceIds > 0 ?
        mojo.internal.kArrayHeaderSize + dimensions.numInterfaceIds * 4 :
        0;
    const paddedInterfaceIdsSize = mojo.internal.align(interfaceIdsSize, 8);
    const totalMessageSize = headerWithPayloadSize + paddedInterfaceIdsSize;

    /** @public {!ArrayBuffer} */
    this.buffer = new ArrayBuffer(totalMessageSize);

    /** @public {!Array<MojoHandle>} */
    this.handles = [];

    const header = new DataView(this.buffer);
    header.setUint32(0, headerSize, mojo.internal.kHostLittleEndian);
    header.setUint32(4, version, mojo.internal.kHostLittleEndian);
    header.setUint32(8, interfaceId, mojo.internal.kHostLittleEndian);
    header.setUint32(12, ordinal, mojo.internal.kHostLittleEndian);
    header.setUint32(16, flags, mojo.internal.kHostLittleEndian);
    header.setUint32(20, 0);  // Padding
    if (version >= 1) {
      mojo.internal.setUint64(header, 24, requestId);
      if (version >= 2) {
        mojo.internal.setUint64(header, 32, BigInt(16));
        mojo.internal.setUint64(header, 40, BigInt(headerWithPayloadSize - 40));
        header.setUint32(
            headerWithPayloadSize, interfaceIdsSize,
            mojo.internal.kHostLittleEndian);
        header.setUint32(
            headerWithPayloadSize + 4, dimensions.numInterfaceIds || 0,
            mojo.internal.kHostLittleEndian);
      }
    }

    /** @private {number} */
    this.nextInterfaceIdIndex_ = 0;

    /** @private {?Uint32Array} */
    this.interfaceIds_ = null;

    if (dimensions.numInterfaceIds) {
      this.interfaceIds_ = new Uint32Array(
          this.buffer, headerWithPayloadSize + mojo.internal.kArrayHeaderSize,
          dimensions.numInterfaceIds);
    }

    /** @private {number} */
    this.nextAllocationOffset_ = headerSize;

    const paramStructData = this.allocate(paramStructSpec.packedSize);
    const encoder =
        new mojo.internal.Encoder(this, paramStructData, {endpoint: sender});
    encoder.encodeStructInline(paramStructSpec, value);
  }

  /**
   * @param {number} numBytes
   * @return {!DataView} A view into the allocated message bytes.
   */
  allocate(numBytes) {
    const alignedSize = mojo.internal.align(numBytes, 8);
    const view =
        new DataView(this.buffer, this.nextAllocationOffset_, alignedSize);
    this.nextAllocationOffset_ += alignedSize;
    return view;
  }
};

/**
 * Additional context to aid in encoding and decoding of message data.
 *
 * @typedef {{
 *   endpoint: ?mojo.internal.interfaceSupport.Endpoint,
 * }}
 */
mojo.internal.MessageContext;

/**
 * Helps encode outgoing messages. Encoders may be created recursively to encode
 * parial message fragments indexed by indirect message offsets, as with encoded
 * arrays and nested structs.
 */
mojo.internal.Encoder = class {
  /**
   * @param {!mojo.internal.Message} message
   * @param {!DataView} data
   * @param {?mojo.internal.MessageContext=} context
   * @public
   */
  constructor(message, data, context = null) {
    /** @const {?mojo.internal.MessageContext} */
    this.context_ = context;

    /** @private {!mojo.internal.Message} */
    this.message_ = message;

    /** @private {!DataView} */
    this.data_ = data;
  }

  encodeBool(byteOffset, bitOffset, value) {
    const oldValue = this.data_.getUint8(byteOffset);
    if (value)
      this.data_.setUint8(byteOffset, oldValue | (1 << bitOffset));
    else
      this.data_.setUint8(byteOffset, oldValue & ~(1 << bitOffset));
  }

  encodeInt8(offset, value) {
    this.data_.setInt8(offset, value);
  }

  encodeUint8(offset, value) {
    this.data_.setUint8(offset, value);
  }

  encodeInt16(offset, value) {
    this.data_.setInt16(offset, value, mojo.internal.kHostLittleEndian);
  }

  encodeUint16(offset, value) {
    this.data_.setUint16(offset, value, mojo.internal.kHostLittleEndian);
  }

  encodeInt32(offset, value) {
    this.data_.setInt32(offset, value, mojo.internal.kHostLittleEndian);
  }

  encodeUint32(offset, value) {
    this.data_.setUint32(offset, value, mojo.internal.kHostLittleEndian);
  }

  encodeInt64(offset, value) {
    mojo.internal.setInt64(this.data_, offset, value);
  }

  encodeUint64(offset, value) {
    mojo.internal.setUint64(this.data_, offset, value);
  }

  encodeFloat(offset, value) {
    this.data_.setFloat32(offset, value, mojo.internal.kHostLittleEndian);
  }

  encodeDouble(offset, value) {
    this.data_.setFloat64(offset, value, mojo.internal.kHostLittleEndian);
  }

  encodeHandle(offset, value) {
    this.encodeUint32(offset, this.message_.handles.length);
    this.message_.handles.push(value);
  }

  encodeAssociatedEndpoint(offset, endpoint) {
    console.assert(
        endpoint.isPendingAssociation, 'expected unbound associated endpoint');
    const sender = this.context_.endpoint;
    const id = sender.associatePeerOfOutgoingEndpoint(endpoint);
    const index = this.message_.nextInterfaceIdIndex_++;
    this.encodeUint32(offset, index);
    this.message_.interfaceIds_[index] = id;
  }

  encodeString(offset, value) {
    if (typeof value !== 'string')
      throw new Error('Unxpected non-string value for string field.');
    this.encodeArray(
        {elementType: mojo.internal.Uint8}, offset,
        mojo.internal.Encoder.stringToUtf8Bytes(value));
  }

  encodeOffset(offset, absoluteOffset) {
    this.encodeUint64(offset, absoluteOffset - this.data_.byteOffset - offset);
  }

  /**
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @param {number} offset
   * @param {!Array|!Uint8Array} value
   */
  encodeArray(arraySpec, offset, value) {
    const arraySize = mojo.internal.computeInlineArraySize(arraySpec, value);
    const arrayData = this.message_.allocate(arraySize);
    const arrayEncoder =
        new mojo.internal.Encoder(this.message_, arrayData, this.context_);
    this.encodeOffset(offset, arrayData.byteOffset);

    arrayEncoder.encodeUint32(0, arraySize);
    arrayEncoder.encodeUint32(4, value.length);
    this.maybeEncodeHasValueBitfield(arraySpec, arrayEncoder, 8, value);

    let byteOffset = 8 +
        mojo.internal.computeHasValueBitfieldSize(arraySpec, value.length);
    if (arraySpec.elementType === mojo.internal.Bool) {
      let bitOffset = 0;
      for (const e of value) {
        arrayEncoder.encodeBool(byteOffset, bitOffset, e);
        bitOffset++;
        if (bitOffset == 8) {
          bitOffset = 0;
          byteOffset++;
        }
      }
    } else {
      for (const e of value) {
        if (e === null) {
          if (!arraySpec.elementNullable) {
            throw new Error(
                'Trying to send a null element in an array of ' +
                'non-nullable elements');
          }
          arraySpec.elementType.$.encodeNull(arrayEncoder, byteOffset);
        } else {
          arraySpec.elementType.$.encode(
              e, arrayEncoder, byteOffset, 0, !!arraySpec.elementNullable);
        }
        byteOffset += arraySpec.elementType.$.arrayElementSize(
            !!arraySpec.elementNullable);
      }
    }
  }

  /**
   * Optionally writes a has-value bitfield to the encoder if necessary. If the
   * arraySpec does not require a has-value bitfield, this method call is
   * noop.
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @param {mojo.internal.Encoder} arrayEncoder
   * @param {number} startOffset
   * @param {!Array|!Uint8Array} value
   */
  maybeEncodeHasValueBitfield(arraySpec, arrayEncoder, startOffset, value) {
    if (!arraySpec.elementNullable ||
        !arraySpec.elementType.$.isValueType) {
      return;
    }

    let bitOffset = 0;
    let byteOffset = startOffset;
    for (const e of value) {
      if (e === null || e === undefined) {
        arrayEncoder.encodeBool(byteOffset, bitOffset, false);
      } else {
        arrayEncoder.encodeBool(byteOffset, bitOffset, true);
      }
      bitOffset++;
      if (bitOffset == 8) {
        bitOffset = 0;
        byteOffset++;
      }
    }
  }

  /**
   * @param {!mojo.internal.MapSpec} mapSpec
   * @param {number} offset
   * @param {!Map|!Object} value
   */
  encodeMap(mapSpec, offset, value) {
    let keys, values;
    if (value.constructor.name == 'Map') {
      keys = Array.from(value.keys());
      values = Array.from(value.values());
    } else {
      keys = Object.keys(value);
      values = keys.map(k => value[k]);
    }

    const mapData = this.message_.allocate(mojo.internal.kMapDataSize);
    const mapEncoder =
        new mojo.internal.Encoder(this.message_, mapData, this.context_);
    this.encodeOffset(offset, mapData.byteOffset);

    mapEncoder.encodeUint32(0, mojo.internal.kMapDataSize);
    mapEncoder.encodeUint32(4, 0);
    mapEncoder.encodeArray({elementType: mapSpec.keyType}, 8, keys);
    mapEncoder.encodeArray(
        {
          elementType: mapSpec.valueType,
          elementNullable: mapSpec.valueNullable,
        },
        16, values);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {number} offset
   * @param {!Object} value
   */
  encodeStruct(structSpec, offset, value) {
    const structData = this.message_.allocate(structSpec.packedSize);
    const structEncoder =
        new mojo.internal.Encoder(this.message_, structData, this.context_);
    this.encodeOffset(offset, structData.byteOffset);
    structEncoder.encodeStructInline(structSpec, value);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {!Object} value
   */
  encodeStructInline(structSpec, value) {
    const versions = structSpec.versions;
    this.encodeUint32(0, structSpec.packedSize);
    this.encodeUint32(4, versions[versions.length - 1].version);
    for (const field of structSpec.fields) {
      const byteOffset = mojo.internal.kStructHeaderSize + field.packedOffset;

      const encodeStructField = (field_value) => {
        field.type.$.encode(field_value, this, byteOffset,
                            field.packedBitOffset, field.nullable);
      };

      // Encode a single optional numeric field into a flag field
      // or a value field.
      if (value && mojo.internal.isNullableValueKindField(field)) {
        const props = field.nullableValueKindProperties;
        const hasValue =
          !mojo.internal.isNullOrUndefined(value[props.originalFieldName]);
        if (props.isPrimary) {
          encodeStructField(hasValue);
        } else if (hasValue) {
          encodeStructField(value[props.originalFieldName]);
        } else {
          // Use `defaultValue` to cover the enum case.
          encodeStructField(field.defaultValue);
        }
        continue;
      }

      if (value && !mojo.internal.isNullOrUndefined(value[field.name])) {
        encodeStructField(value[field.name]);
        continue;
      }

      if (field.defaultValue !== null) {
        encodeStructField(field.defaultValue);
        continue;
      }

      if (field.nullable) {
        field.type.$.encodeNull(this, byteOffset);
        continue;
      }

      throw new Error(
        structSpec.name + ' missing value for non-nullable ' +
        'field "' + field.name + '"');
    }
  }

  /**
   * @param {!mojo.internal.UnionSpec} unionSpec
   * @param {number} offset
   * @param {!Object} value
   */
  encodeUnionAsPointer(unionSpec, offset, value) {
    const unionData = this.message_.allocate(mojo.internal.kUnionDataSize);
    const unionEncoder =
        new mojo.internal.Encoder(this.message_, unionData, this.context_);
    this.encodeOffset(offset, unionData.byteOffset);
    unionEncoder.encodeUnion(unionSpec, /*offset=*/0, value);
  }

  /**
   * @param {!mojo.internal.UnionSpec} unionSpec
   * @param {number} offset
   * @param {!Object} value
   */
  encodeUnion(unionSpec, offset, value) {
    const keys = Object.keys(value);
    if (keys.length !== 1) {
      throw new Error(
          `Value for ${unionSpec.name} must be an Object with a ` +
          'single property named one of: ' +
          Object.keys(unionSpec.fields).join(','));
    }

    const tag = keys[0];
    const field = unionSpec.fields[tag];
    this.encodeUint32(offset, mojo.internal.kUnionDataSize);
    this.encodeUint32(offset + 4, field['ordinal']);
    const fieldByteOffset = offset + mojo.internal.kUnionHeaderSize;
    if (typeof field['type'].$.unionSpec !== 'undefined') {
      // Unions are encoded as pointers when inside unions.
      this.encodeUnionAsPointer(field['type'].$.unionSpec,
                                fieldByteOffset,
                                value[tag]);
      return;
    }
    field['type'].$.encode(
        value[tag], this, fieldByteOffset, 0, field['nullable']);
  }

  /**
   * @param {string} value
   * @return {!Uint8Array}
   */
  static stringToUtf8Bytes(value) {
    if (!mojo.internal.Encoder.textEncoder)
      mojo.internal.Encoder.textEncoder = new TextEncoder('utf-8');
    return mojo.internal.Encoder.textEncoder.encode(value);
  }
};

/** @type {TextEncoder} */
mojo.internal.Encoder.textEncoder = null;

/**
 * Helps decode incoming messages. Decoders may be created recursively to
 * decode partial message fragments indexed by indirect message offsets, as with
 * encoded arrays and nested structs.
 */
mojo.internal.Decoder = class {
  /**
   * @param {!DataView} data
   * @param {!Array<MojoHandle>} handles
   * @param {?mojo.internal.MessageContext=} context
   */
  constructor(data, handles, context = null) {
    /** @private {?mojo.internal.MessageContext} */
    this.context_ = context;

    /** @private {!DataView} */
    this.data_ = data;

    /** @private {!Array<MojoHandle>} */
    this.handles_ = handles;
  }

  decodeBool(byteOffset, bitOffset) {
    return !!(this.data_.getUint8(byteOffset) & (1 << bitOffset));
  }

  decodeInt8(offset) {
    return this.data_.getInt8(offset);
  }

  decodeUint8(offset) {
    return this.data_.getUint8(offset);
  }

  decodeInt16(offset) {
    return this.data_.getInt16(offset, mojo.internal.kHostLittleEndian);
  }

  decodeUint16(offset) {
    return this.data_.getUint16(offset, mojo.internal.kHostLittleEndian);
  }

  decodeInt32(offset) {
    return this.data_.getInt32(offset, mojo.internal.kHostLittleEndian);
  }

  decodeUint32(offset) {
    return this.data_.getUint32(offset, mojo.internal.kHostLittleEndian);
  }

  decodeInt64(offset) {
    return mojo.internal.getInt64(this.data_, offset);
  }

  decodeUint64(offset) {
    return mojo.internal.getUint64(this.data_, offset);
  }

  decodeFloat(offset) {
    return this.data_.getFloat32(offset, mojo.internal.kHostLittleEndian);
  }

  decodeDouble(offset) {
    return this.data_.getFloat64(offset, mojo.internal.kHostLittleEndian);
  }

  decodeHandle(offset) {
    const index = this.data_.getUint32(offset, mojo.internal.kHostLittleEndian);
    if (index == 0xffffffff)
      return null;
    if (index >= this.handles_.length)
      throw new Error('Decoded invalid handle index');
    return this.handles_[index];
  }

  decodeString(offset) {
    const data = this.decodeArray({elementType: mojo.internal.Uint8}, offset);
    if (!data)
      return null;

    if (!mojo.internal.Decoder.textDecoder)
      mojo.internal.Decoder.textDecoder = new TextDecoder('utf-8');
    return mojo.internal.Decoder.textDecoder.decode(
        new Uint8Array(data).buffer);
  }

  decodeOffset(offset) {
    const relativeOffset = this.decodeUint64(offset);
    if (relativeOffset == 0)
      return 0;
    if (relativeOffset > BigInt(Number.MAX_SAFE_INTEGER))
      throw new Error('Mesage offset too large');
    return this.data_.byteOffset + offset + Number(relativeOffset);
  }

  /**
   * @param {!mojo.internal.ArraySpec} arraySpec
   * @return {Array}
   */
  decodeArray(arraySpec, offset) {
    const arrayOffset = this.decodeOffset(offset);
    if (!arrayOffset)
      return null;

    const arrayDecoder = new mojo.internal.Decoder(
        new DataView(this.data_.buffer, arrayOffset), this.handles_,
        this.context_);

    const numElements = arrayDecoder.decodeUint32(4);
    if (!numElements)
      return [];

    // Nullable primitives use a bitfield to represent whether a value at a
    // certain index is set. This is not needed for non-primitive or
    // non-nullable types.
    const isNullableValueType = !!arraySpec.elementNullable &&
        arraySpec.elementType.$.isValueType;
    const elementHasValue = isNullableValueType ? [] : null;

    if (isNullableValueType) {
      let bitfieldByte = 8;
      let bitfieldBit = 0;

      for (let i = 0; i < numElements; ++i) {
        elementHasValue.push(
          arrayDecoder.decodeBool(bitfieldByte, bitfieldBit));
        bitfieldBit++;
        if (bitfieldBit === 8) {
          bitfieldBit = 0;
          bitfieldByte++;
        }
      }
    }

    let byteOffset = 8 +
        mojo.internal.computeHasValueBitfieldSize(arraySpec, numElements);
    const result = [];
    if (arraySpec.elementType === mojo.internal.Bool) {
      for (let i = 0; i < numElements; ++i)
        if (isNullableValueType && !elementHasValue[i]) {
          result.push(null);
        } else {
          result.push(arrayDecoder.decodeBool(byteOffset + (i >> 3), i % 8));
        }
    } else {
      for (let i = 0; i < numElements; ++i) {
        if (isNullableValueType && !elementHasValue[i]) {
          result.push(null);
        } else {
          const element = arraySpec.elementType.$.decode(
            arrayDecoder, byteOffset, 0, !!arraySpec.elementNullable);
          if (element === null && !arraySpec.elementNullable)
            throw new Error('Received unexpected array element');
          result.push(element);
        }
        byteOffset += arraySpec.elementType.$.arrayElementSize(
            !!arraySpec.elementNullable);
      }
    }
    return result;
  }

  /**
   * @param {!mojo.internal.MapSpec} mapSpec
   * @return {Object|Map}
   */
  decodeMap(mapSpec, offset) {
    const mapOffset = this.decodeOffset(offset);
    if (!mapOffset)
      return null;

    const mapDecoder = new mojo.internal.Decoder(
        new DataView(this.data_.buffer, mapOffset), this.handles_,
        this.context_);
    const mapStructSize = mapDecoder.decodeUint32(0);
    const mapStructVersion = mapDecoder.decodeUint32(4);
    if (mapStructSize != mojo.internal.kMapDataSize || mapStructVersion != 0)
      throw new Error('Received invalid map data');

    const keys = mapDecoder.decodeArray({elementType: mapSpec.keyType}, 8);
    const values = mapDecoder.decodeArray(
        {
          elementType: mapSpec.valueType,
          elementNullable: mapSpec.valueNullable
        },
        16);

    if (keys.length != values.length)
      throw new Error('Received invalid map data');
    if (!mapSpec.keyType.$.isValidObjectKeyType) {
      const map = new Map;
      for (let i = 0; i < keys.length; ++i)
        map.set(keys[i], values[i]);
      return map;
    }

    const map = {};
    for (let i = 0; i < keys.length; ++i)
      map[keys[i]] = values[i];
    return map;
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @return {Object}
   */
  decodeStruct(structSpec, offset) {
    const structOffset = this.decodeOffset(offset);
    if (!structOffset)
      return null;

    const decoder = new mojo.internal.Decoder(
        new DataView(this.data_.buffer, structOffset), this.handles_,
        this.context_);
    return decoder.decodeStructInline(structSpec);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {number} size
   * @param {number} version
   * @return {boolean}
   */
  isStructHeaderValid(structSpec, size, version) {
    const versions = structSpec.versions;
    for (let i = versions.length - 1; i >= 0; --i) {
      const info = versions[i];
      if (version > info.version) {
        // If it's newer than the next newest version we know about, the only
        // requirement is that it's at least large enough to decode that next
        // newest version.
        return size >= info.packedSize;
      }
      if (version == info.version) {
        // If it IS the next newest version we know about, expect an exact size
        // match.
        return size == info.packedSize;
      }
    }

    // This should be effectively unreachable, because we always generate info
    // for version 0, and the `version` parameter here is guaranteed in practice
    // to be a non-negative value.
    throw new Error(
        `Impossible version ${version} for struct ${structSpec.name}`);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @return {!Object}
   */
  decodeStructInline(structSpec) {
    const size = this.decodeUint32(0);
    const version = this.decodeUint32(4);
    if (!this.isStructHeaderValid(structSpec, size, version)) {
      throw new Error(
          `Received ${structSpec.name} of invalid size (${size}) and/or ` +
          `version (${version})`);
    }

    const decodeStructField = (structField) => {
      const byteOffset =
        mojo.internal.kStructHeaderSize + structField.packedOffset;
      const value = structField.type.$.decode(
        this, byteOffset, structField.packedBitOffset, !!structField.nullable);

      if (value === null && !structField.nullable) {
        throw new Error(
          `Received ${structSpec.name} with invalid null field ` +
          `"${structField.name}"`)
      }
      return value;
    };

    const result = {};
    for (const field of structSpec.fields) {
      // Decode an optional numeric pair into a single
      // field.
      if (mojo.internal.isNullableValueKindField(field)) {
        const props = field.nullableValueKindProperties;
        if (props.isPrimary && field.minVersion > version) {
          result[props.originalFieldName] = null;
        } else if (props.isPrimary) {
          const hasValue = decodeStructField(field);
          // If the field is null, set it here. If it isn't,
          // the value will be decoded as part of decoding
          // the non-primary field below.
          if (!hasValue) {
            result[props.originalFieldName] = null;
          }
        } else {
          // If the field hasn't been set yet, then it's not
          // null and we need to decode the value.
          if (!(props.originalFieldName in result)) {
            result[props.originalFieldName] =
              decodeStructField(field);
          }
        }
        continue;
      }

      if (field.minVersion > version) {
        result[field.name] = field.defaultValue;
        continue;
      }
      result[field.name] = decodeStructField(field);
    }

    return result;
  }

  /**
   * @param {!mojo.internal.UnionSpec} unionSpec
   * @param {number} offset
   */
  decodeUnionFromPointer(unionSpec, offset) {
    const unionOffset = this.decodeOffset(offset);
    if (!unionOffset)
      return null;

    const decoder = new mojo.internal.Decoder(
        new DataView(this.data_.buffer, unionOffset), this.handles_,
        this.context_);
    return decoder.decodeUnion(unionSpec, 0);
  }

  /**
   * @param {!mojo.internal.UnionSpec} unionSpec
   * @param {number} offset
   */
  decodeUnion(unionSpec, offset) {
    const size = this.decodeUint32(offset);
    if (size === 0)
      return null;

    const ordinal = this.decodeUint32(offset + 4);
    for (const fieldName in unionSpec.fields) {
      const field = unionSpec.fields[fieldName];
      if (field['ordinal'] === ordinal) {
        const fieldValue = (() => {
          const fieldByteOffset = offset + mojo.internal.kUnionHeaderSize;
          // Unions are encoded as pointers when inside other
          // unions.
          if (typeof field['type'].$.unionSpec !== 'undefined') {
            return this.decodeUnionFromPointer(
              field['type'].$.unionSpec, fieldByteOffset);
          }
          return field['type'].$.decode(
            this, fieldByteOffset, 0, field['nullable'])
        })();

        if (fieldValue === null && !field['nullable']) {
          throw new Error(
              `Received ${unionSpec.name} with invalid null ` +
              `field: ${field['name']}`);
        }
        const value = {};
        value[fieldName] = fieldValue;
        return value;
      }
    }
  }

  decodeInterfaceProxy(type, offset) {
    const handle = this.decodeHandle(offset);
    // TODO: support versioning
    if (!handle)
      return null;
    return new type(handle);
  }

  decodeInterfaceRequest(type, offset) {
    const handle = this.decodeHandle(offset);
    if (!handle)
      return null;
    return new type(mojo.internal.interfaceSupport.createEndpoint(handle));
  }

  decodeAssociatedEndpoint(offset) {
    if (!this.context_ || !this.context_.endpoint) {
      throw new Error('cannot deserialize associated endpoint without context');
    }
    const receivingEndpoint = this.context_.endpoint;
    const message = new DataView(this.data_.buffer);
    const interfaceIdsOffset = Number(mojo.internal.getUint64(message, 40));
    const numInterfaceIds = message.getUint32(
        interfaceIdsOffset + 44, mojo.internal.kHostLittleEndian);
    const interfaceIds = new Uint32Array(
        message.buffer,
        interfaceIdsOffset + mojo.internal.kArrayHeaderSize + 40,
        numInterfaceIds);
    const index = this.decodeUint32(offset);
    const interfaceId = interfaceIds[index];
    return new mojo.internal.interfaceSupport.Endpoint(
        receivingEndpoint.router, interfaceId);
  }
};

/** @type {TextDecoder} */
mojo.internal.Decoder.textDecoder = null;

/**
 * @typedef {{
 *   headerSize: number,
 *   headerVersion: number,
 *   interfaceId: number,
 *   ordinal: number,
 *   flags: number,
 *   requestId: number,
 * }}
 */
mojo.internal.MessageHeader;

/**
 * @param {!DataView} data
 * @return {!mojo.internal.MessageHeader}
 */
mojo.internal.deserializeMessageHeader = function(data) {
  const headerSize = data.getUint32(0, mojo.internal.kHostLittleEndian);
  const headerVersion = data.getUint32(4, mojo.internal.kHostLittleEndian);
  if ((headerVersion == 0 &&
       headerSize != mojo.internal.kMessageV0HeaderSize) ||
      (headerVersion == 1 &&
       headerSize != mojo.internal.kMessageV1HeaderSize) ||
      (headerVersion >= 2 &&
       headerSize < mojo.internal.kMessageV2HeaderSize)) {
    throw new Error('Received invalid message header');
  }
  return {
    headerSize,
    headerVersion,
    interfaceId: data.getUint32(8, mojo.internal.kHostLittleEndian),
    ordinal: data.getUint32(12, mojo.internal.kHostLittleEndian),
    flags: data.getUint32(16, mojo.internal.kHostLittleEndian),
    requestId: (headerVersion < 1) ?
        0 :
        data.getUint32(24, mojo.internal.kHostLittleEndian),
  };
};

/**
 * @typedef {{
 *   encode: function(*, !mojo.internal.Encoder, number, number, boolean),
 *   encodeNull: function(!mojo.internal.Encoder, number),
 *   decode: function(!mojo.internal.Decoder, number, number, boolean):*,
 *   computeDimensions:
 *       ((function(*, boolean):!mojo.internal.MessageDimensions)|undefined),
 *   isValidObjectKeyType: boolean,
 *   hasInterfaceId: (boolean|undefined),
 *   arrayElementSize: ((function(boolean):number)|undefined),
 *   arraySpec: (!mojo.internal.ArraySpec|undefined),
 *   mapSpec: (!mojo.internal.MapSpec|undefined),
 *   structSpec: (!mojo.internal.StructSpec|undefined),
 *   isValueType: boolean
 * }}
 */
mojo.internal.MojomTypeInfo;

/**
 * @typedef {{
 *   $: !mojo.internal.MojomTypeInfo
 * }}
 */
mojo.internal.MojomType;

/**
 * @typedef {{
 *   elementType: !mojo.internal.MojomType,
 *   elementNullable: (boolean|undefined)
 * }}
 */
mojo.internal.ArraySpec;

/**
 * @typedef {{
 *   keyType: !mojo.internal.MojomType,
 *   valueType: !mojo.internal.MojomType,
 *   valueNullable: boolean
 * }}
 */
mojo.internal.MapSpec;

// Use a @record, otherwise Closure Compiler will mangle the property names and
// cause runtime errors.
/** @record */
mojo.internal.NullableValueKindProperties = class {
  constructor() {
    /** @export { boolean } */
    this.isPrimary;
    /** @export { (string|undefined) } */
    this.linkedValueFieldName;
    /** @export { string } */
    this.originalFieldName;
  }
};

/**
 * @typedef {{
 *   name: string,
 *   packedOffset: number,
 *   packedBitOffset: number,
 *   type: !mojo.internal.MojomType,
 *   defaultValue: *,
 *   nullable: boolean,
 *   minVersion: number,
 *   nullableValueKindProperties:
 *      (mojo.internal.NullableValueKindProperties|undefined),
 * }}
 */
mojo.internal.StructFieldSpec;

/**
 * @typedef {{
 *   version: number,
 *   packedSize: number,
 * }}
 */
mojo.internal.StructVersionInfo;

/**
 * @typedef {{
 *   name: string,
 *   packedSize: number,
 *   fields: !Array<!mojo.internal.StructFieldSpec>,
 *   versions: !Array<!mojo.internal.StructVersionInfo>,
 * }}
 */
mojo.internal.StructSpec;

/**
 * @typedef {{
 *   name: string,
 *   ordinal: number,
 *   nullable: boolean
 * }}
 */
mojo.internal.UnionFieldSpec;

/**
 * @typedef {{
 *   name: string,
 *   fields: !Object<string, !mojo.internal.UnionFieldSpec>
 * }}
 */
mojo.internal.UnionSpec;

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Bool = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeBool(byteOffset, bitOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      throw new Error('encoding bool null from type is not implemented');
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeBool(byteOffset, bitOffset);
    },
    // Bool has specialized serialize/deserialize logic to bit pack. However,
    // memory allocation is still a single byte.
    arrayElementSize: nullable => 1,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Int8 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeInt8(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeInt8(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt8(byteOffset);
    },
    arrayElementSize: nullable => 1,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Uint8 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeUint8(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeUint8(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint8(byteOffset);
    },
    arrayElementSize: nullable => 1,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Int16 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeInt16(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeInt16(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt16(byteOffset);
    },
    arrayElementSize: nullable => 2,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Uint16 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeUint16(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeUint16(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint16(byteOffset);
    },
    arrayElementSize: nullable => 2,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Int32 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeInt32(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeInt32(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt32(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Uint32 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeUint32(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeUint32(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint32(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Int64 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeInt64(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeInt64(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt64(byteOffset);
    },
    arrayElementSize: nullable => 8,
    // TS Compiler does not allow Object maps to have bigint keys.
    isValidObjectKeyType: false,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Uint64 = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeUint64(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeUint64(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint64(byteOffset);
    },
    arrayElementSize: nullable => 8,
    // TS Compiler does not allow Object maps to have bigint keys.
    isValidObjectKeyType: false,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Float = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeFloat(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeFloat(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeFloat(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Double = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeDouble(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeDouble(byteOffset, 0);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeDouble(byteOffset);
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: true,
    isValueType: true,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Handle = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeHandle(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {
      encoder.encodeUint32(byteOffset, 0xffffffff);
    },
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeHandle(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: false,
    isValueType: false,
  },
};

/**
 * @const {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.String = {
  $: {
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeString(byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeString(byteOffset);
    },
    computeDimensions: function(value, nullable) {
      const size = mojo.internal.computeTotalArraySize(
          {elementType: mojo.internal.Uint8},
          mojo.internal.Encoder.stringToUtf8Bytes(value));
      return {size};
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: true,
    isValueType: false,
  }
};

/**
 * @param {!mojo.internal.MojomType} elementType
 * @param {boolean} elementNullable
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Array = function(elementType, elementNullable) {
  /** @type {!mojo.internal.ArraySpec} */
  const arraySpec = {
    elementType: elementType,
    elementNullable: elementNullable,
  };
  return {
    $: {
      arraySpec: arraySpec,
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        encoder.encodeArray(arraySpec, byteOffset, value);
      },
      encodeNull: function(encoder, byteOffset) {},
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return decoder.decodeArray(arraySpec, byteOffset);
      },
      computeDimensions: function(value, nullable) {
        return {size: mojo.internal.computeTotalArraySize(arraySpec, value)};
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
      isValueType: false,
    },
  };
};

/**
 * @param {!mojo.internal.MojomType} keyType
 * @param {!mojo.internal.MojomType} valueType
 * @param {boolean} valueNullable
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Map = function(keyType, valueType, valueNullable) {
  /** @type {!mojo.internal.MapSpec} */
  const mapSpec = {
    keyType: keyType,
    valueType: valueType,
    valueNullable: valueNullable,
  };
  return {
    $: {
      mapSpec: mapSpec,
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        encoder.encodeMap(mapSpec, byteOffset, value);
      },
      encodeNull: function(encoder, byteOffset) {},
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return decoder.decodeMap(mapSpec, byteOffset);
      },
      computeDimensions: function(value, nullable) {
        const keys =
            (value.constructor.name == 'Map') ? Array.from(value.keys())
                                              : Object.keys(value);
        const values =
            (value.constructor.name == 'Map') ? Array.from(value.values())
                                              : keys.map(k => value[k]);
        // Size of map is equal to kMapDataSize + 8-byte aligned array for keys
        // + (not necessarily 8-byte aligned) array for values.
        const size = mojo.internal.kMapDataSize +
            mojo.internal.align(
                mojo.internal.computeTotalArraySize(
                    {elementType: keyType}, keys),
              8) +
            mojo.internal.computeTotalArraySize(
                {
                  elementType: valueType,
                  elementNullable: valueNullable,
                },
                values);
        return {size};
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
      isValueType: false,
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.Enum = function() {
  return {
    $: {
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        // TODO: Do some sender-side error checking on the input value.
        encoder.encodeUint32(byteOffset, value);
      },
      encodeNull: function(encoder, byteOffset) {},
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        const value = decoder.decodeInt32(byteOffset);
        // TODO: validate
        return value;
      },
      arrayElementSize: nullable => 4,
      isValidObjectKeyType: true,
      isValueType: true,
    },
  };
};

/**
 * @param {string} name
 * @param {number} packedOffset
 * @param {number} packedBitOffset
 * @param {!mojo.internal.MojomType} type
 * @param {*} defaultValue
 * @param {boolean} nullable
 * @param {number=} minVersion
 * @param {mojo.internal.NullableValueKindProperties=}
     nullableValueKindProperties
 * @return {!mojo.internal.StructFieldSpec}
 * @export
 */
mojo.internal.StructField = function(
  name, packedOffset, packedBitOffset, type, defaultValue, nullable,
  minVersion = 0, nullableValueKindProperties = undefined) {
  return {
    name: name,
    packedOffset: packedOffset,
    packedBitOffset: packedBitOffset,
    type: type,
    defaultValue: defaultValue,
    nullable: nullable,
    minVersion: minVersion,
    nullableValueKindProperties: nullableValueKindProperties,
  };
};

/**
 * @param {!Object} objectToBlessAsType
 * @param {string} name
 * @param {!Array<!mojo.internal.StructFieldSpec>} fields
 * @param {Array<!Array<number>>=} versionData
 * @export
 */
mojo.internal.Struct = function(
    objectToBlessAsType, name, fields, versionData) {
  const versions = versionData.map(v => ({version: v[0], packedSize: v[1]}));
  const packedSize = versions[versions.length - 1].packedSize;
  const structSpec = {name, packedSize, fields, versions};
  objectToBlessAsType.$ = {
    structSpec: structSpec,
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeStruct(structSpec, byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeStruct(structSpec, byteOffset);
    },
    computeDimensions: function(value, nullable) {
      return mojo.internal.computeStructDimensions(structSpec, value);
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: false,
  };
};

/**
 * Bridges typemapped types to mojo types. The adapter includes a function which
 * will convert a mapped type to mojo type and vice versa.
 * @export
 */
mojo.internal.TypemapAdapter = class {
  constructor(toMojoTypeFn, toMappedTypeFn) {
    this.toMojoTypeFn = toMojoTypeFn;
    this.toMappedTypeFn = toMappedTypeFn;
  }
}

/**
 * @param {!Object} objectToBlessAsType
 * @param {string} name
 * @param {!mojo.internal.TypemapAdapter} typemapAdapter
 * @param {!Array<!mojo.internal.StructFieldSpec>} fields
 * @param {Array<!Array<number>>=} versionData
 * @export
 */
mojo.internal.TypemappedStruct = function(
    objectToBlessAsType, name, typemapAdapter, fields, versionData) {
  const versions = versionData.map(v => ({version: v[0], packedSize: v[1]}));
  const packedSize = versions[versions.length - 1].packedSize;
  const structSpec = {name, packedSize, fields, versions};
  objectToBlessAsType.$ = {
    structSpec: structSpec,
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      const mojoType = typemapAdapter.toMojoTypeFn(value);
      encoder.encodeStruct(structSpec, byteOffset, mojoType);
    },
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      const mojoType = decoder.decodeStruct(structSpec, byteOffset);
      if (mojoType === null || mojoType === undefined) {
        return mojoType;
      }
      return typemapAdapter.toMappedTypeFn(mojoType);
    },
    computeDimensions: function(value, nullable) {
      const mojoType = typemapAdapter.toMojoTypeFn(value);
      return mojo.internal.computeStructDimensions(structSpec, mojoType);
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: false,
  };
};

/**
 * @param {!mojo.internal.MojomType} structMojomType
 * @return {!Function}
 * @export
 */
mojo.internal.createStructDeserializer = function(structMojomType) {
  return function(dataView) {
    if (structMojomType.$ == undefined ||
        structMojomType.$.structSpec == undefined) {
      throw new Error('Invalid struct mojom type!');
    }
    const decoder = new mojo.internal.Decoder(dataView, []);
    return decoder.decodeStructInline(structMojomType.$.structSpec);
  };
};

/**
 * @param {!Object} objectToBlessAsUnion
 * @param {string} name
 * @param {!Object} fields
 * @export
 */
mojo.internal.Union = function(objectToBlessAsUnion, name, fields) {
  /** @type {!mojo.internal.UnionSpec} */
  const unionSpec = {
    name: name,
    fields: fields,
  };
  objectToBlessAsUnion.$ = {
    unionSpec: unionSpec,
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeUnion(unionSpec, byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUnion(unionSpec, byteOffset);
    },
    computeDimensions: function(value, nullable) {
      return mojo.internal.computeUnionDimensions(unionSpec, nullable, value);
    },
    arrayElementSize: nullable => (nullable ? 8 : 16),
    isValidObjectKeyType: false,
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.InterfaceProxy = function(type) {
  return {
    $: {
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        const endpoint = value.proxy.unbind();
        console.assert(endpoint, `unexpected null ${type.name}`);

        const pipe = endpoint.releasePipe();
        encoder.encodeHandle(byteOffset, pipe);
        encoder.encodeUint32(byteOffset + 4, 0);  // TODO: Support versioning
      },
      encodeNull: function(encoder, byteOffset) {
        encoder.encodeUint32(byteOffset, 0xffffffff);
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return decoder.decodeInterfaceProxy(type, byteOffset);
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
      isValueType: false,
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.InterfaceRequest = function(type) {
  return {
    $: {
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        if (!value.handle)
          throw new Error('Unexpected null ' + type.name);

        encoder.encodeHandle(byteOffset, value.handle.releasePipe());
      },
      encodeNull: function(encoder, byteOffset) {
        encoder.encodeUint32(byteOffset, 0xffffffff);
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return decoder.decodeInterfaceRequest(type, byteOffset);
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
      isValueType: false,
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.AssociatedInterfaceProxy = function(type) {
  return {
    $: {
      type: type,
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        console.assert(
            value.proxy.endpoint && value.proxy.endpoint.isPendingAssociation,
            `expected ${type.name} to be associated and unbound`);
        encoder.encodeAssociatedEndpoint(byteOffset, value.proxy.endpoint);
        encoder.encodeUint32(byteOffset + 4, 0);
      },
      encodeNull: function(encoder, byteOffset) {
        encoder.encodeUint32(byteOffset, 0xffffffff);
        encoder.encodeUint32(byteOffset + 4, 0);
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return new type(decoder.decodeAssociatedEndpoint(byteOffset));
      },
      arrayElementSize: _ => {
        throw new Error('Arrays of associated endpoints are not yet supported');
      },
      isValidObjectKeyType: false,
      hasInterfaceId: true,
      isValueType: false,
    },
  };
};

/**
 * @return {!mojo.internal.MojomType}
 * @export
 */
mojo.internal.AssociatedInterfaceRequest = function(type) {
  return {
    $: {
      type: type,
      encode: function(value, encoder, byteOffset, bitOffset, nullable) {
        console.assert(
            value.handle && value.handle.isPendingAssociation,
            `expected ${type.name} to be associated and unbound`);

        encoder.encodeAssociatedEndpoint(byteOffset, value.handle);
      },
      encodeNull: function(encoder, byteOffset) {
        encoder.encodeUint32(byteOffset, 0xffffffff);
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return new type(decoder.decodeAssociatedEndpoint(byteOffset));
      },
      arrayElementSize: _ => {
        throw new Error('Arrays of associated endpoints are not yet supported');
      },
      isValidObjectKeyType: false,
      hasInterfaceId: true,
      isValueType: false,
    },
  };
};
