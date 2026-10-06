// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for `mojoBase.mojom` types.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let mojoBaseMojomBigBuffer = "mojoBase.mojom.BigBuffer"
    static let mojoBaseMojomBigBufferBytes = "mojoBase.mojom.BigBuffer.bytes"
    static let mojoBaseMojomBigBufferSharedMemory = "mojoBase.mojom.BigBuffer.sharedMemory"
    static let mojoBaseMojomBigBufferInvalidBuffer = "mojoBase.mojom.BigBuffer.invalidBuffer"
    static let mojoBaseMojomBigBufferSharedMemoryRegion =
        "mojoBase.mojom.BigBufferSharedMemoryRegion"
    static let mojoBaseMojomBigString16 = "mojoBase.mojom.BigString16"
    static let mojoBaseMojomBigString = "mojoBase.mojom.BigString"
    static let mojoBaseMojomString16 = "mojoBase.mojom.String16"
    static let mojoBaseMojomUint128 = "mojoBase.mojom.Uint128"
}

public let mojoBaseMojomBuiltins: [String: ILType] = [
    CommonMojoStrings.mojoBaseMojomString16: .jsMojoBaseMojomString16Constructor,
    CommonMojoStrings.mojoBaseMojomBigBufferBytes: .jsMojoBaseMojomBigBufferBytesConstructor,
    CommonMojoStrings.mojoBaseMojomBigBufferSharedMemory:
        .jsMojoBaseMojomBigBufferSharedMemoryConstructor,
    CommonMojoStrings.mojoBaseMojomBigBufferInvalidBuffer:
        .jsMojoBaseMojomBigBufferInvalidBufferConstructor,
    CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion:
        .jsMojoBaseMojomBigBufferSharedMemoryRegionConstructor,
    CommonMojoStrings.mojoBaseMojomBigString16: .jsMojoBaseMojomBigString16Constructor,
    CommonMojoStrings.mojoBaseMojomUint128: .jsMojoBaseMojomUint128,
]

public let mojoBaseMojomCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoMojoBaseMojomBigBufferBytesGenerator, 1),
    (MojoMojoBaseMojomBigBufferSharedMemoryRegionGenerator, 1),
    (MojoMojoBaseMojomString16Generator, 1),
]

public let mojoBaseMojomObjectGroups: [ObjectGroup] = [
    .mojoBaseMojomBigBuffer,
    .mojoBaseMojomBigBufferSharedMemoryRegion,
    .mojoBaseMojomBigString16,
    .mojoBaseMojomString16,
    .mojoBaseMojomUint128,
]

extension ILType {
    public static let jsMojoBaseMojomBigBuffer: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigBuffer)
    public static let jsMojoBaseMojomBigBufferBytesConstructor: ILType = .constructor(
        [.plain(.createJsArrayType(ofElementType: .jsUint8))] => .jsMojoBaseMojomBigBuffer)
    public static let jsMojoBaseMojomBigBufferSharedMemoryConstructor: ILType = .constructor(
        [.plain(.jsMojoBaseMojomBigBufferSharedMemoryRegion)] => .jsMojoBaseMojomBigBuffer)
    public static let jsMojoBaseMojomBigBufferInvalidBufferConstructor: ILType = .constructor(
        [.plain(.boolean)] => .jsMojoBaseMojomBigBuffer)

    public static let jsMojoBaseMojomBigBufferSharedMemoryRegion: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion,
        withProperties: ["bufferHandle", "size"])
    public static let jsMojoBaseMojomBigBufferSharedMemoryRegionConstructor: ILType = .constructor(
        [.plain(.jsMojoSharedBufferHandle), .integer] => .jsMojoBaseMojomBigBufferSharedMemoryRegion
    )
    public static let jsMojoBaseMojomBigString16: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigString16, withProperties: ["data"])
    public static let jsMojoBaseMojomBigString16Constructor: ILType = .constructor(
        [.plain(.jsMojoBaseMojomBigBuffer)] => .jsMojoBaseMojomBigString16
    )
    public static let jsMojoBaseMojomBigString: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigString, withProperties: ["data"])
    public static let jsMojoBaseMojomBigStringConstructor: ILType = .constructor(
        [.plain(.jsMojoBaseMojomBigBuffer)] => .jsMojoBaseMojomBigString)
    public static let jsMojoBaseMojomString16: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomString16, withProperties: ["data"])
    public static let jsMojoBaseMojomString16Constructor: ILType = .constructor(
        [.plain(.createJsArrayType(ofElementType: .jsUint16))] => .jsMojoBaseMojomString16
    )
    public static let jsMojoBaseMojomUint128: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomUint128, withProperties: ["high", "low"])
}

extension ObjectGroup {
    public static let mojoBaseMojomBigBuffer = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigBuffer,
        instanceType: .jsMojoBaseMojomBigBuffer,
        properties: [:],
        methods: [:]
    )
    public static let mojoBaseMojomBigBufferSharedMemoryRegion = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion,
        instanceType: .jsMojoBaseMojomBigBufferSharedMemoryRegion,
        properties: [
            "bufferHandle": .jsMojoSharedBufferHandle,
            "size": .integer,
        ],
        methods: [:]
    )
    public static let mojoBaseMojomBigString = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigString,
        instanceType: .jsMojoBaseMojomBigString,
        properties: [
            "data": .jsMojoBaseMojomBigBuffer
        ],
        methods: [:]
    )
    public static let mojoBaseMojomBigString16 = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigString16,
        instanceType: .jsMojoBaseMojomBigString16,
        properties: [
            "data": .jsMojoBaseMojomBigBuffer
        ],
        methods: [:]
    )
    public static let mojoBaseMojomString16 = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomString16,
        instanceType: .jsMojoBaseMojomString16,
        properties: [
            "data": .createJsArrayType(ofElementType: .jsUint16)
        ],
        methods: [:]
    )
    public static let mojoBaseMojomUint128 = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomUint128,
        instanceType: .jsMojoBaseMojomUint128,
        properties: [
            "high": .integer,
            "low": .integer,
        ],
        methods: [:]
    )
}

public let MojoMojoBaseMojomBigBufferBytesGenerator = CodeGenerator(
    "MojoMojoBaseMojomBigBufferBytesGenerator",
    inputs: .one,
    produces: [.createJsArrayType(ofElementType: .jsUint8)]
) { b, _ in
    var elements: [Variable] = [b.findOrGenerateType(.jsUint8)]
    b.createArray(with: elements)
}

public let MojoMojoBaseMojomBigBufferSharedMemoryRegionGenerator = CodeGenerator(
    "MojoMojoBaseMojomBigBufferSharedBufferGenerator",
    inputs: .one,
    produces: [.jsMojoBaseMojomBigBufferSharedMemoryRegion]
) { b, _ in
    let mojo = b.createNamedVariable(forBuiltin: "Mojo")
    let numBytes = b.loadInt(Int64.random(in: 0...Int64(UInt32.max)))
    let sharedBufferResult = b.callMethod("createSharedBuffer", on: mojo, withArgs: [numBytes])
    let handle = b.getProperty("handle", of: sharedBufferResult)

    let mapResult = b.callMethod("mapBuffer", on: handle, withArgs: [b.loadInt(0), numBytes])
    let buffer = b.getProperty("buffer", of: mapResult)
    let uint8ArrayConstructor = b.createNamedVariable(forBuiltin: "Uint8Array")
    let view = b.construct(uint8ArrayConstructor, withArgs: [buffer])
    let randomByte = b.loadInt(Int64.random(in: 0...255))
    b.callMethod("fill", on: view, withArgs: [randomByte])

    let regionConstructor = b.createNamedVariable(
        forBuiltin: CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion)
    b.construct(regionConstructor, withArgs: [handle, numBytes])
}

public let MojoMojoBaseMojomString16Generator = CodeGenerator(
    "MojoMojoBaseMojomString16Generator",
    inputs: .one,
    produces: [.jsMojoBaseMojomString16]
) { b, _ in
    // Ideally the CodeGenerator would use a string from the JavaScript
    // program. Such a string would be represented by a `Variable` object, and
    // there is no clean way to grab the underlying string value from a
    // `Variable` object. So, instead, generate a random uint16 values.
    var elements: [Variable] = []
    for _ in (0..<Int64.random(in: 1...30)) {
       elements.append(b.findOrGenerateType(.jsUint16))
    }
    let array = b.createArray(with: elements)
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.mojoBaseMojomString16)
    b.construct(constructor, withArgs: [array])
}
