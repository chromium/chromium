// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for `mojoBase.mojom` types.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let mojoBaseMojomBigBufferUnion = "mojoBase.mojom.BigBuffer"
    static let mojoBaseMojomBigBufferUnionBytes = "mojoBase.mojom.BigBuffer.bytes"
    static let mojoBaseMojomBigBufferUnionSharedMemory = "mojoBase.mojom.BigBuffer.sharedMemory"
    static let mojoBaseMojomBigBufferUnionInvalidBuffer = "mojoBase.mojom.BigBuffer.invalidBuffer"
    static let mojoBaseMojomBigBufferSharedMemoryRegion =
        "mojoBase.mojom.BigBufferSharedMemoryRegion"
    static let mojoBaseMojomBigString16 = "mojoBase.mojom.BigString16"
    static let mojoBaseMojomBigString = "mojoBase.mojom.BigString"
    static let mojoBaseMojomString16 = "mojoBase.mojom.String16"
    static let mojoBaseMojomUint128 = "mojoBase.mojom.Uint128"
}

public let mojoBaseMojomBuiltins: [String: ILType] = [
    CommonMojoStrings.mojoBaseMojomString16: .jsMojoBaseMojomString16Constructor,
    CommonMojoStrings.mojoBaseMojomBigBufferUnionBytes:
        .jsMojoBaseMojomBigBufferUnionBytesConstructor,
    CommonMojoStrings.mojoBaseMojomBigBufferUnionSharedMemory:
        .jsMojoBaseMojomBigBufferUnionSharedMemoryConstructor,
    CommonMojoStrings.mojoBaseMojomBigBufferUnionInvalidBuffer:
        .jsMojoBaseMojomBigBufferUnionInvalidBufferConstructor,
    CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion:
        .jsMojoBaseMojomBigBufferSharedMemoryRegionConstructor,
    CommonMojoStrings.mojoBaseMojomBigString16: .jsMojoBaseMojomBigString16Constructor,
    CommonMojoStrings.mojoBaseMojomUint128: .jsMojoBaseMojomUint128,
]

public let mojoBaseMojomCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoMojoBaseMojomBigBufferUnionBytesGenerator, 1),
    (MojoMojoBaseMojomBigBufferSharedMemoryRegionGenerator, 1),
    (MojoMojoBaseMojomString16Generator, 1),
]

public let mojoBaseMojomObjectGroups: [ObjectGroup] = [
    .mojoBaseMojomBigBufferUnion,
    .mojoBaseMojomBigBufferSharedMemoryRegion,
    .mojoBaseMojomBigString16,
    .mojoBaseMojomString16,
    .mojoBaseMojomUint128,
]

extension ILType {
    public static let jsMojoBaseMojomBigBufferUnion: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigBufferUnion)
    public static let jsMojoBaseMojomBigBufferUnionBytesConstructor: ILType = .constructor(
        [.plain(.createJsArrayType(ofElementType: .jsUint8))] => .jsMojoBaseMojomBigBufferUnion)
    public static let jsMojoBaseMojomBigBufferUnionSharedMemoryConstructor: ILType = .constructor(
        [.plain(.jsMojoBaseMojomBigBufferSharedMemoryRegion)] => .jsMojoBaseMojomBigBufferUnion)
    public static let jsMojoBaseMojomBigBufferUnionInvalidBufferConstructor: ILType = .constructor(
        [.plain(.boolean)] => .jsMojoBaseMojomBigBufferUnion)

    public static let jsMojoBaseMojomBigBufferSharedMemoryRegion: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion,
        withProperties: ["bufferHandle", "size"])
    public static let jsMojoBaseMojomBigBufferSharedMemoryRegionConstructor: ILType = .constructor(
        [.plain(.jsMojoSharedBufferHandle), .integer] => .jsMojoBaseMojomBigBufferSharedMemoryRegion
    )
    public static let jsMojoBaseMojomBigString16: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigString16, withProperties: ["data"])
    public static let jsMojoBaseMojomBigString16Constructor: ILType = .constructor(
        [.plain(.jsMojoBaseMojomBigBufferUnion)] => .jsMojoBaseMojomBigString16
    )
    public static let jsMojoBaseMojomBigString: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigString, withProperties: ["data"])
    public static let jsMojoBaseMojomBigStringConstructor: ILType = .constructor(
        [.plain(.jsMojoBaseMojomBigBufferUnion)] => .jsMojoBaseMojomBigString)
    public static let jsMojoBaseMojomString16: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomString16, withProperties: ["data"])
    public static let jsMojoBaseMojomString16Constructor: ILType = .constructor(
        [.plain(.createJsArrayType(ofElementType: .jsUint16))] => .jsMojoBaseMojomString16
    )
    public static let jsMojoBaseMojomUint128: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomUint128, withProperties: ["high", "low"])
}

extension ObjectGroup {
    public static let mojoBaseMojomBigBufferUnion = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigBufferUnion,
        instanceType: .jsMojoBaseMojomBigBufferUnion,
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
            "data": .jsMojoBaseMojomBigBufferUnion
        ],
        methods: [:]
    )
    public static let mojoBaseMojomBigString16 = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigString16,
        instanceType: .jsMojoBaseMojomBigString16,
        properties: [
            "data": .jsMojoBaseMojomBigBufferUnion
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

public let MojoMojoBaseMojomBigBufferUnionBytesGenerator = CodeGenerator(
    "MojoMojoBaseMojomBigBufferUnionBytesGenerator",
    inputs: .one,
    produces: [.createJsArrayType(ofElementType: .jsUint8)]
) { b, _ in
    let count: Int
    switch Int.random(in: 0..<4) {
    case 0: count = 1
    case 1: count = Int.random(in: 2...16)
    case 2: count = 32
    default: count = 64
    }
    let elements = (0..<count).map { _ in b.findOrGenerateType(.jsUint8) }
    b.createArray(with: elements)
}

public let MojoMojoBaseMojomBigBufferSharedMemoryRegionGenerator = CodeGenerator(
    "MojoMojoBaseMojomBigBufferSharedBufferGenerator",
    inputs: .one,
    produces: [.jsMojoBaseMojomBigBufferSharedMemoryRegion]
) { b, _ in
    let mojo = b.createNamedVariable(forBuiltin: "Mojo")
    // BigBuffer switches from inline bytes to shared memory at 64 KiB, so most
    // buffers are around that size. Larger ones cost time to map and fill, and
    // `Mojo.createSharedBuffer` is capped anyway (see `mojoCommonCodePrefix`).
    let size: Int64
    switch Int.random(in: 0..<10) {
    case 0: size = Int64.random(in: 0...16)
    case 1...6: size = Int64.random(in: 1...(128 * 1024))
    case 7, 8:
        size = chooseUniform(
            from: [4095, 4096, 4097, 32767, 32768, 32769, 65535, 65536, 65537, 131072])
    default: size = Int64.random(in: 1...Int64(mojoMaxSharedBufferSize))
    }
    let numBytes = b.loadInt(size)
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
    // The size in the message is what the browser has to validate against the
    // buffer, so it sometimes differs from the size of the buffer.
    var claimedSize = numBytes
    if probability(0.2) {
        claimedSize = b.loadInt(
            chooseUniform(from: [0, size - 1, size + 1, size * 2, Int64(UInt32.max)]))
    }
    b.construct(regionConstructor, withArgs: [handle, claimedSize])
}

private let string16Vocabulary = [
    "db", "store", "idx", "a", "b.txt", "sub", "", ".", "..", "a/b", ".crswap", "user", "pass",
    "a\u{0}b", "\u{0}", "sub\u{0}.txt", "a\r\nb: c", "\u{01}\u{1F}\u{7F}", ".git", ".ssh",
    "a/./b",
]

public let MojoMojoBaseMojomString16Generator = CodeGenerator(
    "MojoMojoBaseMojomString16Generator",
    inputs: .one,
    produces: [.jsMojoBaseMojomString16]
) { b, _ in
    var elements: [Variable] = []
    if probability(0.75) {
        let word = chooseUniform(from: string16Vocabulary)
        for u in word.utf16 {
            elements.append(b.loadInt(Int64(u), customName: "Uint16"))
        }
    } else {
        for _ in (0..<Int64.random(in: 1...20)) {
            elements.append(b.findOrGenerateType(.jsUint16))
        }
    }
    let array = b.createArray(with: elements)
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.mojoBaseMojomString16)
    b.construct(constructor, withArgs: [array])
}
