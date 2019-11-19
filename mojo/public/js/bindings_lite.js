// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

goog.provide('mojo.internal');

// "self" is always defined as opposed to "this", which isn't defined in
// modules, or "window", which isn't defined in workers.
/** @const {!Object} */
mojo.internal.globalScope = self;

/**
 * This is effectively the same as goog.provide, but it's made available under
 * the mojo.internal namespace to avoid potential collisions in certain
 * compilation environments.
 *
 * @param {string} namespace
 * @export
 */
mojo.internal.exportModule = function(namespace) {
  let current = mojo.internal.globalScope;
  const parts = namespace.split('.');

  for (let part; parts.length && (part = parts.shift());) {
    if (!current[part])
      current[part] = {};
    current = current[part];
  }
};

/** @const {number} */
mojo.internal.kArrayHeaderSize = 8;

/** @const {number} */
mojo.internal.kStructHeaderSize = 8;

/** @const {number} */
mojo.internal.kUnionDataSize = 16;

/** @const {number} */
mojo.internal.kMessageV0HeaderSize = 24;

/** @const {number} */
mojo.internal.kMessageV1HeaderSize = 32;

/** @const {number} */
mojo.internal.kMapDataSize = 24;

/** @const {number} */
mojo.internal.kEncodedInvalidHandleValue = 0xffffffff;

/** @const {number} */
mojo.internal.kMessageFlagExpectsResponse = 1 << 0;

/** @const {number} */
mojo.internal.kMessageFlagIsResponse = 1 << 1;

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
 * @param {number} value
 */
mojo.internal.setInt64 = function(dataView, byteOffset, value) {
  if (mojo.internal.kHostLittleEndian) {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setInt32(
        byteOffset + 4, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
  } else {
    dataView.setInt32(
        byteOffset, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  }
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @param {number} value
 */
mojo.internal.setUint64 = function(dataView, byteOffset, value) {
  if (mojo.internal.kHostLittleEndian) {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
  } else {
    dataView.setUint32(
        byteOffset, Number(BigInt(value) >> BigInt(32)),
        mojo.internal.kHostLittleEndian);
    dataView.setUint32(
        byteOffset + 4, Number(BigInt(value) & BigInt(0xffffffff)),
        mojo.internal.kHostLittleEndian);
  }
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @return {number}
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
  const value = (BigInt(high) << BigInt(32)) | BigInt(low);
  if (value <= BigInt(Number.MAX_SAFE_INTEGER) &&
      value >= BigInt(Number.MIN_SAFE_INTEGER)) {
    return Number(value);
  }
  return value;
};

/**
 * This computes the total amount of buffer space required to hold a struct
 * value and all its fields, including indirect objects like arrays, structs,
 * and nullable unions.
 *
 * @param {!mojo.internal.StructSpec} structSpec
 * @param {!Object} value
 * @return {number}
 */
mojo.internal.computeTotalStructSize = function(structSpec, value) {
  let size = mojo.internal.kStructHeaderSize + structSpec.packedSize;
  for (const field of structSpec.fields) {
    const fieldValue = value[field.name];
    if (field.type.$.computePayloadSize &&
        !mojo.internal.isNullOrUndefined(fieldValue)) {
      size += mojo.internal.align(
          field.type.$.computePayloadSize(fieldValue, field.nullable), 8);
    }
  }
  return size;
};

/**
 * @param {!mojo.internal.UnionSpec} unionSpec
 * @param {!Object} value
 * @return {number}
 */
mojo.internal.computeTotalUnionSize = function(unionSpec, nullable, value) {
  // Unions are normally inlined since they're always a fixed width of 16
  // bytes, but nullable union-typed fields require indirection. Hence this
  // unique special case where a union field requires additional storage
  // beyond the struct's own packed field data only when it's nullable.
  let size = nullable ? mojo.internal.kUnionDataSize : 0;

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
    if (field['type'].$.unionSpec) {
      // Nested unions are always encoded with indirection, which we induce by
      // claiming the field is nullable even if it's not.
      size += mojo.internal.align(
          field['type'].$.computePayloadSize(fieldValue, true /* nullable */),
          8);
    } else if (field['type'].$.computePayloadSize) {
      size += mojo.internal.align(
          field['type'].$.computePayloadSize(fieldValue, field['nullable']), 8);
    }
  }

  return size;
};

/**
 * @param {!mojo.internal.ArraySpec} arraySpec
 * @param {!Array|!Uint8Array} value
 * @return {number}
 */
mojo.internal.computeInlineArraySize = function(arraySpec, value) {
  if (arraySpec.elementType === mojo.internal.Bool) {
    return mojo.internal.kArrayHeaderSize + (value.length + 7) >> 3;
  } else {
    return mojo.internal.kArrayHeaderSize +
        value.length *
        arraySpec.elementType.$.arrayElementSize(!!arraySpec.elementNullable);
  }
};

/**
 * @param {!mojo.internal.ArraySpec} arraySpec
 * @param {!Array|!Uint8Array} value
 * @return {number}
 */
mojo.internal.computeTotalArraySize = function(arraySpec, value) {
  const inlineSize = mojo.internal.computeInlineArraySize(arraySpec, value);
  if (!arraySpec.elementType.$.computePayloadSize)
    return inlineSize;

  let totalSize = inlineSize;
  for (let elementValue of value) {
    if (!mojo.internal.isNullOrUndefined(elementValue)) {
      totalSize += mojo.internal.align(
          arraySpec.elementType.$.computePayloadSize(
              elementValue, !!arraySpec.elementNullable),
          8);
    }
  }

  return totalSize;
};

/**
 * @param {!DataView} dataView
 * @param {number} byteOffset
 * @return {number}
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
  const value = (BigInt(high) << BigInt(32)) | BigInt(low);
  if (value <= BigInt(Number.MAX_SAFE_INTEGER))
    return Number(value);
  return value;
};

/** Owns an outgoing message buffer and facilitates serialization. */
mojo.internal.Message = class {
  /**
   * @param {number} flags
   * @param {number} ordinal
   * @param {number} requestId
   * @param {!mojo.internal.StructSpec} paramStructSpec
   * @param {!Object} value
   * @private
   */
  constructor(flags, ordinal, requestId, paramStructSpec, value) {
    let headerSize, version;
    if ((flags &
         (mojo.internal.kMessageFlagExpectsResponse |
          mojo.internal.kMessageFlagIsResponse)) == 0) {
      headerSize = mojo.internal.kMessageV0HeaderSize;
      version = 0;
    } else {
      headerSize = mojo.internal.kMessageV1HeaderSize;
      version = 1;
    }

    const totalMessageSize = headerSize +
        mojo.internal.computeTotalStructSize(paramStructSpec, value);

    /** @public {!ArrayBuffer} */
    this.buffer = new ArrayBuffer(totalMessageSize);

    /** @public {!Array<MojoHandle>} */
    this.handles = [];

    const header = new DataView(this.buffer);
    header.setUint32(0, headerSize, mojo.internal.kHostLittleEndian);
    header.setUint32(4, version, mojo.internal.kHostLittleEndian);
    header.setUint32(8, 0);  // Interface ID (only for associated interfaces)
    header.setUint32(12, ordinal, mojo.internal.kHostLittleEndian);
    header.setUint32(16, flags, mojo.internal.kHostLittleEndian);
    header.setUint32(20, 0);  // Padding
    if (version > 0)
      mojo.internal.setUint64(header, 24, requestId);

    /** @private {number} */
    this.nextAllocationOffset_ = headerSize;

    const paramStructData = this.allocate(
        mojo.internal.kStructHeaderSize + paramStructSpec.packedSize);
    const encoder = new mojo.internal.Encoder(this, paramStructData);
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
 * Helps encode outgoing messages. Encoders may be created recursively to encode
 * parial message fragments indexed by indirect message offsets, as with encoded
 * arrays and nested structs.
 */
mojo.internal.Encoder = class {
  /**
   * @param {!mojo.internal.Message} message
   * @param {!DataView} data
   * @public
   */
  constructor(message, data) {
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
    const arrayEncoder = new mojo.internal.Encoder(this.message_, arrayData);
    this.encodeOffset(offset, arrayData.byteOffset);

    arrayEncoder.encodeUint32(0, arraySize);
    arrayEncoder.encodeUint32(4, value.length);

    let byteOffset = 8;
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
        }
        arraySpec.elementType.$.encode(
            e, arrayEncoder, byteOffset, 0, !!arraySpec.elementNullable);
        byteOffset += arraySpec.elementType.$.arrayElementSize(
            !!arraySpec.elementNullable);
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
    if (value instanceof Map) {
      keys = Array.from(value.keys());
      values = Array.from(value.values());
    } else {
      keys = Object.keys(value);
      values = keys.map(k => value[k]);
    }

    const mapData = this.message_.allocate(mojo.internal.kMapDataSize);
    const mapEncoder = new mojo.internal.Encoder(this.message_, mapData);
    this.encodeOffset(offset, mapData.byteOffset);

    mapEncoder.encodeUint32(0, mojo.internal.kMapDataSize);
    mapEncoder.encodeUint32(4, 0);
    mapEncoder.encodeArray({elementType: mapSpec.keyType}, 8, keys);
    mapEncoder.encodeArray(
        {
          elementType: mapSpec.valueType,
          elementNullable: mapSpec.valueNullable
        },
        16, values);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {number} offset
   * @param {!Object} value
   */
  encodeStruct(structSpec, offset, value) {
    const structData = this.message_.allocate(
        mojo.internal.kStructHeaderSize + structSpec.packedSize);
    const structEncoder = new mojo.internal.Encoder(this.message_, structData);
    this.encodeOffset(offset, structData.byteOffset);
    structEncoder.encodeStructInline(structSpec, value);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @param {!Object} value
   */
  encodeStructInline(structSpec, value) {
    this.encodeUint32(
        0, mojo.internal.kStructHeaderSize + structSpec.packedSize);
    this.encodeUint32(4, 0);  // TODO: Support versioning.
    for (const field of structSpec.fields) {
      const byteOffset = mojo.internal.kStructHeaderSize + field.packedOffset;
      if (!value || !(value instanceof Object) ||
          mojo.internal.isNullOrUndefined(value[field.name])) {
        if (!field.nullable) {
          throw new Error(
              structSpec.name + ' missing value for non-nullable ' +
              'field "' + field.name + '"');
        }
        field.type.$.encodeNull(this, byteOffset);
        continue;
      }

      field.type.$.encode(
          value[field.name], this, byteOffset, field.packedBitOffset,
          field.nullable);
    }
  }

  /**
   * @param {!mojo.internal.UnionSpec} unionSpec
   * @param {number} offset
   * @param {boolean} nullable
   * @param {!Object} value
   */
  encodeUnion(unionSpec, offset, nullable, value) {
    let unionEncoder = this;
    if (nullable) {
      const unionData = this.message_.allocate(mojo.internal.kUnionDataSize);
      this.encodeOffset(offset, unionData.byteOffset);
      offset = 0;
      unionEncoder = new mojo.internal.Encoder(this.message_, unionData);
    }

    const keys = Object.keys(value);
    if (keys.length !== 1) {
      throw new Error(
          `Value for ${unionSpec.name} must be an Object with a ` +
          'single property named one of: ' +
          Object.keys(unionSpec.fields).join(','));
    }

    const tag = keys[0];
    const field = unionSpec.fields[tag];
    unionEncoder.encodeUint32(offset, mojo.internal.kUnionDataSize);
    unionEncoder.encodeUint32(offset + 4, field['ordinal']);
    field['type'].$.encode(
        value[tag], unionEncoder, offset + 8, 0, field['nullable']);
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
   */
  constructor(data, handles) {
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
    if (!mojo.internal.Decoder.textDecoder)
      mojo.internal.Decoder.textDecoder = new TextDecoder('utf-8');
    return mojo.internal.Decoder.textDecoder.decode(
        new Uint8Array(this.decodeArray(
                           {
                             elementType: mojo.internal.Uint8,
                           },
                           offset))
            .buffer);
  }

  decodeOffset(offset) {
    const relativeOffset = this.decodeUint64(offset);
    if (relativeOffset == 0)
      return 0;
    return this.data_.byteOffset + offset + relativeOffset;
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
        new DataView(this.data_.buffer, arrayOffset), this.handles_);

    const size = arrayDecoder.decodeUint32(0);
    const numElements = arrayDecoder.decodeUint32(4);
    if (!numElements)
      return [];

    const result = [];
    if (arraySpec.elementType === mojo.internal.Bool) {
      for (let i = 0; i < numElements; ++i)
        result.push(arrayDecoder.decodeBool(8 + (i >> 3), i % 8));
    } else {
      let byteOffset = 8;
      for (let i = 0; i < numElements; ++i) {
        const element = arraySpec.elementType.$.decode(
            arrayDecoder, byteOffset, 0, !!arraySpec.elementNullable);
        if (element === null && !arraySpec.elementNullable)
          throw new Error('Received unexpected array element');
        result.push(element);
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
        new DataView(this.data_.buffer, mapOffset), this.handles_);
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
        new DataView(this.data_.buffer, structOffset), this.handles_);
    return decoder.decodeStructInline(structSpec);
  }

  /**
   * @param {!mojo.internal.StructSpec} structSpec
   * @return {!Object}
   */
  decodeStructInline(structSpec) {
    const size = this.decodeUint32(0);
    const version = this.decodeUint32(4);
    const result = {};
    for (const field of structSpec.fields) {
      const byteOffset = mojo.internal.kStructHeaderSize + field.packedOffset;
      const value = field.type.$.decode(
          this, byteOffset, field.packedBitOffset, !!field.nullable);
      if (value === null && !field.nullable) {
        throw new Error(
            'Received ' + structSpec.name + ' with invalid null field ' +
            '"' + field.name + '"')
      }
      result[field.name] = value;
    }
    return result;
  }

  /**
   * @param {!mojo.internal.UnionSpec} unionSpec
   * @param {number} offset
   * @param {boolean} nullable
   */
  decodeUnion(unionSpec, offset, nullable) {
    let unionDecoder = this;
    if (nullable) {
      const unionOffset = this.decodeOffset(offset);
      if (!unionOffset)
        return null;
      unionDecoder = new mojo.internal.Decoder(
          new DataView(this.data_.buffer, unionOffset), this.handles_);
      offset = 0;
    }

    const ordinal = unionDecoder.decodeUint32(offset + 4);
    for (const fieldName in unionSpec.fields) {
      const field = unionSpec.fields[fieldName];
      if (field['ordinal'] === ordinal) {
        const fieldValue = field['type'].$.decode(
            unionDecoder, offset + 8, 0, field['nullable']);
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
    const version = this.decodeUint32(offset + 4);  // TODO: support versioning
    if (!handle)
      return null;
    return new type(handle);
  }

  decodeInterfaceRequest(type, offset) {
    const handle = this.decodeHandle(offset);
    if (!handle)
      return null;
    return new type(handle);
  }
};

/** @type {TextDecoder} */
mojo.internal.Decoder.textDecoder = null;

/**
 * @param {!MojoHandle} handle
 * @param {number} ordinal
 * @param {number} requestId
 * @param {number} flags
 * @param {!mojo.internal.MojomType} paramStruct
 * @param {!Object} value
 */
mojo.internal.serializeAndSendMessage = function(
    handle, ordinal, requestId, flags, paramStruct, value) {
  const message = new mojo.internal.Message(
      flags, ordinal, requestId,
      /** @type {!mojo.internal.StructSpec} */ (paramStruct.$.structSpec),
      value);
  handle.writeMessage(message.buffer, message.handles);
};

/**
 * @param {!DataView} data
 * @return {{
 *     headerSize: number,
 *     ordinal: number,
 *     flags: number,
 *     requestId: number,
 * }}
 */
mojo.internal.deserializeMessageHeader = function(data) {
  const headerSize = data.getUint32(0, mojo.internal.kHostLittleEndian);
  const headerVersion = data.getUint32(4, mojo.internal.kHostLittleEndian);
  if ((headerVersion == 0 &&
       headerSize != mojo.internal.kMessageV0HeaderSize) ||
      (headerVersion == 1 &&
       headerSize != mojo.internal.kMessageV1HeaderSize) ||
      headerVersion > 2) {
    throw new Error('Received invalid message header');
  }
  if (headerVersion == 2)
    throw new Error('v2 messages not yet supported');
  const header = {
    headerSize: headerSize,
    ordinal: data.getUint32(12, mojo.internal.kHostLittleEndian),
    flags: data.getUint32(16, mojo.internal.kHostLittleEndian),
  };
  if (headerVersion > 0)
    header.requestId = data.getUint32(24, mojo.internal.kHostLittleEndian);
  else
    header.requestId = 0;
  return header;
};

/**
 * @typedef {{
 *   encode: function(*, !mojo.internal.Encoder, number, number, boolean),
 *   encodeNull: ((function(!mojo.internal.Encoder, number))|undefined),
 *   decode: function(!mojo.internal.Decoder, number, number, boolean):*,
 *   computePayloadSize: ((function(*, boolean):number)|undefined),
 *   isValidObjectKeyType: boolean,
 *   arrayElementSize: ((function(boolean):number)|undefined),
 *   arraySpec: (!mojo.internal.ArraySpec|undefined),
 *   mapSpec: (!mojo.internal.MapSpec|undefined),
 *   structSpec: (!mojo.internal.StructSpec|undefined),
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

/**
 * @typedef {{
 *   name: string,
 *   packedOffset: number,
 *   packedBitOffset: number,
 *   type: !mojo.internal.MojomType,
 *   defaultValue: *,
 *   nullable: boolean,
 * }}
 */
mojo.internal.StructFieldSpec;

/**
 * @typedef {{
 *   name: string,
 *   packedSize: number,
 *   fields: !Array<!mojo.internal.StructFieldSpec>,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeBool(byteOffset, bitOffset);
    },
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt8(byteOffset);
    },
    arrayElementSize: nullable => 1,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint8(byteOffset);
    },
    arrayElementSize: nullable => 1,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt16(byteOffset);
    },
    arrayElementSize: nullable => 2,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint16(byteOffset);
    },
    arrayElementSize: nullable => 2,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt32(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint32(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeInt64(byteOffset);
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUint64(byteOffset);
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeFloat(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: true,
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
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeDouble(byteOffset);
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: true,
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
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeHandle(byteOffset);
    },
    arrayElementSize: nullable => 4,
    isValidObjectKeyType: false,
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
    computePayloadSize: function(value, nullable) {
      return mojo.internal.computeTotalArraySize(
          {elementType: mojo.internal.Uint8},
          mojo.internal.Encoder.stringToUtf8Bytes(value));
    },
    arrayElementSize: nullable => 8,
    isValidObjectKeyType: true,
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
      computePayloadSize: function(value, nullable) {
        return mojo.internal.computeTotalArraySize(arraySpec, value);
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
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
      computePayloadSize: function(value, nullable) {
        const keys = (value instanceof Map) ? Array.from(value.keys()) :
                                              Object.keys(value);
        const values = (value instanceof Map) ? Array.from(value.values()) :
                                                keys.map(k => value[k]);

        return mojo.internal.kMapDataSize +
            mojo.internal.computeTotalArraySize({elementType: keyType}, keys) +
            mojo.internal.computeTotalArraySize(
                {
                  elementType: valueType,
                  elementNullable: valueNullable,
                },
                values);
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
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
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        const value = decoder.decodeInt32(byteOffset);
        // TODO: validate
        return value;
      },
      arrayElementSize: nullable => 4,
      isValidObjectKeyType: true,
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
 * @return {!mojo.internal.StructFieldSpec}
 * @export
 */
mojo.internal.StructField = function(
    name, packedOffset, packedBitOffset, type, defaultValue, nullable) {
  return {
    name: name,
    packedOffset: packedOffset,
    packedBitOffset: packedBitOffset,
    type: type,
    defaultValue: defaultValue,
    nullable: nullable,
  };
};

/**
 * @param {!Object} objectToBlessAsType
 * @param {string} name
 * @param {number} packedSize
 * @param {!Array<!mojo.internal.StructFieldSpec>} fields
 * @export
 */
mojo.internal.Struct = function(objectToBlessAsType, name, packedSize, fields) {
  /** @type {!mojo.internal.StructSpec} */
  const structSpec = {
    name: name,
    packedSize: packedSize,
    fields: fields,
  };
  objectToBlessAsType.$ = {
    structSpec: structSpec,
    encode: function(value, encoder, byteOffset, bitOffset, nullable) {
      encoder.encodeStruct(structSpec, byteOffset, value);
    },
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeStruct(structSpec, byteOffset);
    },
    computePayloadSize: function(value, nullable) {
      return mojo.internal.computeTotalStructSize(structSpec, value);
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
      throw new Error("Invalid struct mojom type!");
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
      encoder.encodeUnion(unionSpec, byteOffset, nullable, value);
    },
    encodeNull: function(encoder, byteOffset) {},
    decode: function(decoder, byteOffset, bitOffset, nullable) {
      return decoder.decodeUnion(unionSpec, byteOffset, nullable);
    },
    computePayloadSize: function(value, nullable) {
      return mojo.internal.computeTotalUnionSize(unionSpec, nullable, value);
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
        if (!(value instanceof type))
          throw new Error('Invalid proxy type. Expected ' + type.name);
        if (!value.proxy.handle)
          throw new Error('Unexpected null ' + type.name);

        encoder.encodeHandle(byteOffset, value.proxy.handle);
        encoder.encodeUint32(byteOffset + 4, 0);  // TODO: Support versioning
        value.proxy.unbind();
      },
      encodeNull: function(encoder, byteOffset) {
        encoder.encodeUint32(byteOffset, 0xffffffff);
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return decoder.decodeInterfaceProxy(type, byteOffset);
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
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
        if (!(value instanceof type))
          throw new Error('Invalid request type. Expected ' + type.name);
        if (!value.handle)
          throw new Error('Unexpected null ' + type.name);
        encoder.encodeHandle(byteOffset, value.handle);
      },
      encodeNull: function(encoder, byteOffset) {
        encoder.encodeUint32(byteOffset, 0xffffffff);
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        return decoder.decodeInterfaceRequest(type, byteOffset);
      },
      arrayElementSize: nullable => 8,
      isValidObjectKeyType: false,
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
        throw new Error('Associated interfaces not supported yet.');
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        throw new Error('Associated interfaces not supported yet.');
      },
      isValidObjectKeyType: false,
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
        throw new Error('Associated interfaces not supported yet.');
      },
      decode: function(decoder, byteOffset, bitOffset, nullable) {
        throw new Error('Associated interfaces not supported yet.');
      },
      isValidObjectKeyType: false,
    },
  };
};
