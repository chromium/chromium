// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for the `Mojo` JavaScript API: the `Mojo` builtin itself, the
/// shared buffer and data pipe handles it vends, and the results and options
/// bags of its methods.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let mojo = "Mojo"
    static let mojoSharedBufferHandle = "MojoSharedBufferHandle"
    static let mojoDataPipeProducerHandle = "MojoDataPipeProducerHandle"
    static let mojoDataPipeConsumerHandle = "MojoDataPipeConsumerHandle"
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
    CommonMojoStrings.mojo: .jsMojo
]

public let mojoCoreObjectGroups: [ObjectGroup] = [
    .mojo,
    .mojoSharedBufferHandle,
    .mojoDataPipeProducerHandle,
    .mojoDataPipeConsumerHandle,
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
