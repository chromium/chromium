// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for the `Mojo` JavaScript API: the `Mojo` builtin itself, the
/// handles it vends or that JS receives from the browser, and the results and
/// options bags of its methods.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let mojo = "Mojo"
    static let mojoSharedBufferHandle = "MojoSharedBufferHandle"
    static let mojoDataPipeProducerHandle = "MojoDataPipeProducerHandle"
    static let mojoDataPipeConsumerHandle = "MojoDataPipeConsumerHandle"
    static let mojoPlatformHandle = "MojoPlatformHandle"
    static let mojoCreateSharedBufferResult = "MojoCreateSharedBufferResult"
    static let mojoCreateDataPipeOptions = "MojoCreateDataPipeOptions"
    static let mojoCreateDataPipeResult = "MojoCreateDataPipeResult"
    static let mojoWriteDataOptions = "MojoWriteDataOptions"
    static let mojoWriteDataResult = "MojoWriteDataResult"
    static let mojoReadDataOptions = "MojoReadDataOptions"
    static let mojoReadDataResult = "MojoReadDataResult"
    static let mojoDiscardDataOptions = "MojoDiscardDataOptions"
}

public let mojoCoreBuiltins: [String: ILType] = [
    CommonMojoStrings.mojo: .jsMojo,
    "mojoSameOriginUrl": .function([.string] => .jsString),
]

public let mojoCoreCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoDataPipeConsumerGenerator, mojoDataPipeGeneratorWeight),
    (MojoDataPipeProducerGenerator, mojoDataPipeGeneratorWeight),
]

public let mojoCoreObjectGroups: [ObjectGroup] = [
    .mojo,
    .mojoSharedBufferHandle,
    .mojoDataPipeProducerHandle,
    .mojoDataPipeConsumerHandle,
    .mojoPlatformHandle,
    .mojoCreateSharedBufferResult,
    .mojoCreateDataPipeResult,
    .mojoWriteDataResult,
    .mojoReadDataResult,
]

public let mojoCoreOptionsBags: [OptionsBag] = [
    .mojoCreateDataPipeOptions,
    .mojoWriteDataOptions,
    .mojoReadDataOptions,
    .mojoDiscardDataOptions,
]

extension ILType {
    public static let jsMojo: ILType = .object(
        ofGroup: CommonMojoStrings.mojo,
        withMethods: ["createSharedBuffer", "createDataPipe"]
    )
    /// Although there is only one `MojoHandle` type defined in the IDL, we
    /// define separate types for the SharedBuffer, DataPipeProducer, and
    /// DataPipeConsumer functionalities. If Fuzzilli has what should only be
    /// treated as a DataPipeConsumer, this approach avoids Fuzzilli from
    /// calling irrelevant methods such as `writeData` that exist on the
    /// `MojoHandle` type.
    public static let jsMojoSharedBufferHandle: ILType = .object(
        ofGroup: CommonMojoStrings.mojoSharedBufferHandle)
    public static let jsMojoDataPipeProducerHandle: ILType = .object(
        ofGroup: CommonMojoStrings.mojoDataPipeProducerHandle,
        withMethods: ["close", "writeData"]
    )
    public static let jsMojoDataPipeConsumerHandle: ILType = .object(
        ofGroup: CommonMojoStrings.mojoDataPipeConsumerHandle,
        withMethods: ["close", "readData", "queryData", "discardData"]
    )
    /// A `handle<platform>`. JS cannot create one; it can only receive one in
    /// a message from the browser. Only `close` is exposed. The other
    /// `MojoHandle` methods fail on a wrapped platform handle.
    public static let jsMojoPlatformHandle: ILType = .object(
        ofGroup: CommonMojoStrings.mojoPlatformHandle,
        withMethods: ["close"]
    )
    public static let jsMojoCreateSharedBufferResult: ILType = .object(
        ofGroup: CommonMojoStrings.mojoCreateSharedBufferResult,
        withProperties: ["result", "handle"]
    )
    public static let jsMojoCreateDataPipeOptions: ILType = OptionsBag.mojoCreateDataPipeOptions
        .group.instanceType
    public static let jsMojoCreateDataPipeResult: ILType = .object(
        ofGroup: CommonMojoStrings.mojoCreateDataPipeResult,
        withProperties: ["result", "producer", "consumer"]
    )
    public static let jsMojoWriteDataOptions: ILType = OptionsBag.mojoWriteDataOptions.group
        .instanceType
    public static let jsMojoWriteDataResult: ILType = .object(
        ofGroup: CommonMojoStrings.mojoWriteDataResult,
        withProperties: ["result", "numBytes"]
    )
    public static let jsMojoReadDataOptions: ILType = OptionsBag.mojoReadDataOptions.group
        .instanceType
    public static let jsMojoReadDataResult: ILType = .object(
        ofGroup: CommonMojoStrings.mojoReadDataResult,
        withProperties: ["result", "numBytes"]
    )
    public static let jsMojoDiscardDataOptions: ILType = OptionsBag.mojoDiscardDataOptions.group
        .instanceType
}

extension ObjectGroup {
    public static let mojo = ObjectGroup(
        name: CommonMojoStrings.mojo,
        instanceType: .jsMojo,
        properties: [:],
        methods: [
            "createSharedBuffer": [.integer] => .jsMojoCreateSharedBufferResult,
            "createDataPipe": [.plain(.jsMojoCreateDataPipeOptions)] => .jsMojoCreateDataPipeResult,
        ]
    )
    public static let mojoSharedBufferHandle = ObjectGroup(
        name: CommonMojoStrings.mojoSharedBufferHandle,
        instanceType: .jsMojoSharedBufferHandle,
        properties: [:],
        methods: [:]
    )
    public static let mojoDataPipeProducerHandle = ObjectGroup(
        name: CommonMojoStrings.mojoDataPipeProducerHandle,
        instanceType: .jsMojoDataPipeProducerHandle,
        properties: [:],
        methods: [
            "close": [] => .undefined,
            "writeData": [
                .plain(.jsBufferSource),
                .either(.jsMojoWriteDataOptions, .undefined),
            ] => .jsMojoWriteDataResult,
        ]
    )
    public static let mojoDataPipeConsumerHandle = ObjectGroup(
        name: CommonMojoStrings.mojoDataPipeConsumerHandle,
        instanceType: .jsMojoDataPipeConsumerHandle,
        properties: [:],
        methods: [
            "close": [] => .undefined,
            "queryData": [] => .jsMojoReadDataResult,
            "discardData": [
                .integer,
                .either(.jsMojoDiscardDataOptions, .undefined),
            ] => .jsMojoReadDataResult,
            "readData": [
                .plain(.jsBufferSource),
                .either(.jsMojoReadDataOptions, .undefined),
            ] => .jsMojoReadDataResult,
        ]
    )
    public static let mojoPlatformHandle = ObjectGroup(
        name: CommonMojoStrings.mojoPlatformHandle,
        instanceType: .jsMojoPlatformHandle,
        properties: [:],
        methods: [
            "close": [] => .undefined
        ]
    )
    public static let mojoCreateSharedBufferResult = ObjectGroup(
        name: CommonMojoStrings.mojoCreateSharedBufferResult,
        instanceType: .jsMojoCreateSharedBufferResult,
        properties: [
            "result": .integer,
            "handle": .jsMojoSharedBufferHandle,
        ],
        methods: [:]
    )
    public static let mojoCreateDataPipeResult = ObjectGroup(
        name: CommonMojoStrings.mojoCreateDataPipeResult,
        instanceType: .jsMojoCreateDataPipeResult,
        properties: [
            "result": .integer,
            "producer": .jsMojoDataPipeProducerHandle,
            "consumer": .jsMojoDataPipeConsumerHandle,
        ],
        methods: [:]
    )
    public static let mojoWriteDataResult = ObjectGroup(
        name: CommonMojoStrings.mojoWriteDataResult,
        instanceType: .jsMojoWriteDataResult,
        properties: [
            "result": .integer,
            "numBytes": .integer,
        ],
        methods: [:]
    )
    public static let mojoReadDataResult = ObjectGroup(
        name: CommonMojoStrings.mojoReadDataResult,
        instanceType: .jsMojoReadDataResult,
        properties: [
            "result": .integer,
            "numBytes": .integer,
        ],
        methods: [:]
    )
}

extension OptionsBag {
    public static let mojoCreateDataPipeOptions = OptionsBag(
        name: CommonMojoStrings.mojoCreateDataPipeOptions,
        properties: [
            "elementNumBytes": .integer,
            "capacityNumBytes": .integer,
        ],
        selectionMode: .anySubset
    )
    public static let mojoDiscardDataOptions = OptionsBag(
        name: CommonMojoStrings.mojoDiscardDataOptions,
        properties: [
            "allOrNone": .boolean
        ],
        selectionMode: .anySubset,
    )
    public static let mojoReadDataOptions = OptionsBag(
        name: CommonMojoStrings.mojoReadDataOptions,
        properties: [
            "allOrNone": .boolean,
            "peek": .boolean,
        ],
        selectionMode: .anySubset,
    )
    public static let mojoWriteDataOptions = OptionsBag(
        name: CommonMojoStrings.mojoWriteDataOptions,
        properties: [
            "allOrNone": .boolean
        ],
        selectionMode: .anySubset,
    )
}

private func createMojoDataPipe(
    _ b: ProgramBuilder, elementNumBytes: Int64, capacityNumBytes: Int64
) -> (producer: Variable, consumer: Variable) {
    let mojo = b.createNamedVariable(forBuiltin: CommonMojoStrings.mojo)
    let elem = b.loadInt(elementNumBytes)
    let cap = b.loadInt(capacityNumBytes)
    let opts = b.createObject(with: [
        "elementNumBytes": elem,
        "capacityNumBytes": cap,
    ])
    let pipe = b.callMethod("createDataPipe", on: mojo, withArgs: [opts])
    let producer = b.getProperty("producer", of: pipe, isReceiverOptional: true)
    let consumer = b.getProperty("consumer", of: pipe, isReceiverOptional: true)
    b.hide(pipe)
    b.hide(opts)
    b.hide(cap)
    b.hide(elem)
    b.hide(mojo)
    return (producer, consumer)
}

private func chooseMojoDataPipeElementSize() -> Int64 {
    return probability(0.85) ? 1 : chooseUniform(from: [2, 3, 4, 17, 4095, 11984])
}

/// Creates a populated `MojoDataPipeConsumerHandle` by writing 0B, small,
/// boundary, or >32KiB multi-chunk bytes into its producer and closing the
/// producer (always when `closeProducer == true`, 85% when `nil`).
@discardableResult
public func emitMojoDataPipeConsumer(
    _ b: ProgramBuilder, byteCount: Int64? = nil, closeProducer: Bool? = nil
) -> Variable {
    let elementNumBytes = byteCount == nil ? chooseMojoDataPipeElementSize() : 1
    let dataSize: Int64
    if let explicit = byteCount {
        dataSize = explicit
    } else if elementNumBytes == 1 {
        let roll = Int.random(in: 0..<20)
        switch roll {
        case 0..<3:
            dataSize = 0
        case 3..<11:
            dataSize = chooseUniform(from: [1, 7, 64, 512, 1024])
        case 11..<16:
            dataSize = chooseUniform(from: [4095, 4096, 4097, 32767, 32768, 32769])
        default:
            dataSize = chooseUniform(from: [40960, 65536, 70000])
        }
    } else {
        dataSize = probability(0.25) ? 0 : (probability(0.65) ? elementNumBytes : elementNumBytes * 2)
    }
    let minCapacity = max(dataSize, elementNumBytes * 4)
    let capacityNumBytes: Int64 =
        elementNumBytes == 1
        ? (dataSize > 65536 ? 131072 : max(minCapacity, chooseUniform(from: [64, 4096, 32768, 65536])))
        : elementNumBytes * chooseUniform(from: [4, 16, 64])
    let (producer, consumer) = createMojoDataPipe(
        b, elementNumBytes: elementNumBytes, capacityNumBytes: capacityNumBytes)
    if dataSize > 0 {
        let uint8Ctor = b.createNamedVariable(forBuiltin: "Uint8Array")
        let sizeVar = b.loadInt(dataSize)
        let buf = b.construct(uint8Ctor, withArgs: [sizeVar])
        let fillByte = b.loadInt(Int64.random(in: 1...255))
        b.callMethod("fill", on: buf, withArgs: [fillByte], isReceiverOptional: true)
        b.callMethod("writeData", on: producer, withArgs: [buf], isReceiverOptional: true)
        b.hide(fillByte)
        b.hide(buf)
        b.hide(sizeVar)
        b.hide(uint8Ctor)
    }
    if closeProducer ?? probability(0.85) {
        b.callMethod("close", on: producer, withArgs: [], isReceiverOptional: true)
        b.hide(producer)
    }
    return consumer
}

/// Generates a populated `MojoDataPipeConsumerHandle` by creating a data pipe,
/// writing 0B, small, boundary, or >32KiB multi-chunk bytes into its producer,
/// and usually closing the producer so readers see data followed by EOF.
public let MojoDataPipeConsumerGenerator = CodeGenerator(
    "MojoDataPipeConsumerGenerator",
    produces: [.jsMojoDataPipeConsumerHandle]
) { b in
    _ = emitMojoDataPipeConsumer(b)
}

/// Generates a `MojoDataPipeProducerHandle` paired with a visible consumer so
/// browser-written data can be read back from the pipe.
public let MojoDataPipeProducerGenerator = CodeGenerator(
    "MojoDataPipeProducerGenerator",
    produces: [.jsMojoDataPipeProducerHandle]
) { b in
    let elementNumBytes = chooseMojoDataPipeElementSize()
    let capacityNumBytes =
        elementNumBytes == 1
        ? chooseUniform(from: [64, 4096, 32768, 65536])
        : elementNumBytes * chooseUniform(from: [4, 16, 64])
    let _ = createMojoDataPipe(
        b, elementNumBytes: elementNumBytes, capacityNumBytes: capacityNumBytes)
}
