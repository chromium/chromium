// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// This file contains public definitions of non-interface specific Mojo types,
/// constructors, and CodeGenerators. It does not provide a standalone profile
/// that can be used for fuzzing (i.e., `--profile=mojoCommon` is invalid).

/// Types defined in this file are only used by generated profiles if they are
/// listed in `IGNORED_TYPES` in
/// `mojo/public/tools/bindings/generators/mojom_fuzzilli_generator.py`. When a
/// type is added to `IGNORED_TYPES`, the generator skips emitting code for it
/// in per-interface profiles and assumes it is provided globally here. If a
/// type is defined here but NOT listed in `IGNORED_TYPES`, the generator will
/// generate duplicate definitions in the target profile, leading to
/// compilation errors.
///
/// Naming Conventions:
/// 1. Group names:
///    - Module and type name, seperated by dots (e.g.
///      "mojoBase.mojom.String16", "url.mojom.Url").
/// 2. `ILType` names:
///    - The unique name is the namespace concatenated with the type name, in
///      PascalCase (e.g. "MojoBaseMojomString16", "UrlMojomUrl",
///      "SkiaMojomBitmapN32").
///    - ILType: `js<UniqueName>` (e.g. `jsMojoBaseMojomString16`).
///    - Constructors: `js<UniqueName>Constructor`.
/// 4. `ObjectGroup` and `OptionsBag` names:
///    - Namespace concatenated with the type name, in camelCase (e.g.
///      "urlMojomUrl")
/// 5. CodeGenerator names:
///    - `Mojo<namespace><type name>Generator` (e.g. `MojoUrlMojomUrlGenerator`)

public enum CommonMojoStrings {
    static let boolElement = "BoolElement"
    static let int8Element = "Int8Element"
    static let int16Element = "Int16Element"
    static let int32Element = "Int32Element"
    static let int64Element = "Int64Element"
    static let uint8Element = "Uint8Element"
    static let uint16Element = "Uint16Element"
    static let uint32Element = "Uint32Element"
    static let uint64Element = "Uint64Element"
    static let floatElement = "FloatElement"
    static let stringElement = "StringElement"

    // mojo
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

    // mojoBase
    static let mojoBaseMojomBigBuffer = "mojoBase.mojom.BigBuffer"
    static let mojoBaseMojomBigBufferSharedMemoryRegion =
        "mojoBase.mojom.BigBufferSharedMemoryRegion"
    static let mojoBaseMojomBigString16 = "mojoBase.mojom.BigString16"
    static let mojoBaseMojomBigString = "mojoBase.mojom.BigString"
    static let mojoBaseMojomString16 = "mojoBase.mojom.String16"
    static let mojoBaseMojomUint128 = "mojoBase.mojom.Uint128"

    // skia
    static let skiaMojomBitmapN32ImageInfo = "skia.mojom.BitmapN32ImageInfo"
    static let skiaMojomAlphaType = "skia.mojom.AlphaType"

    // url
    static let urlMojomUrl = "url.mojom.Url"
    static let urlMojomSchemeHostPort = "url.mojom.SchemeHostPort"
}

public let commonMojoBuiltins: [String: ILType] = [
    // mojo
    CommonMojoStrings.mojo: .jsMojo,

    // mojoBase
    CommonMojoStrings.mojoBaseMojomString16: .jsMojoBaseMojomString16Constructor,

    CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion:
        .jsMojoBaseMojomBigBufferSharedMemoryRegionConstructor,
    CommonMojoStrings.mojoBaseMojomBigString16: .jsMojoBaseMojomBigString16Constructor,
    CommonMojoStrings.mojoBaseMojomUint128: .jsMojoBaseMojomUint128,

    // skia
    CommonMojoStrings.skiaMojomBitmapN32ImageInfo: .jsSkiaMojomBitmapN32ImageInfoConstructor,

    //url
    CommonMojoStrings.urlMojomSchemeHostPort: .jsUrlMojomSchemeHostPortConstructor,
    CommonMojoStrings.urlMojomUrl: .jsUrlMojomUrlConstructor,
]

public let commonMojoCodeGenerators: [(CodeGenerator, Int)] = [
    // mojo
    (MojoObjectLiteralNoopGenerator, 1),
    (MojoBufferSourceGenerator, 1),

    // mojoBase
    (MojoMojoBaseMojomBigBufferBytesGenerator, 1),
    (MojoMojoBaseMojomBigBufferSharedMemoryRegionGenerator, 1),
    (MojoMojoBaseMojomString16Generator, 1),

    // skia
    (MojoSkiaMojomColorTransferFunctionArrayGenerator, 1),
    (MojoSkiaMojomColorToXyzMatrixArrayGenerator, 1),

    // url
    (MojoUrlMojomUrlSchemeHostPortGenerator, 1),
    (MojoUrlMojomUrlGenerator, 1),
]

public let commonMojoObjectGroups: [ObjectGroup] = [
    .boolElement,
    .int8Element,
    .int16Element,
    .int32Element,
    .int64Element,
    .uint8Element,
    .uint16Element,
    .uint32Element,
    .uint64Element,
    .floatElement,
    .stringElement,

    // mojo
    .mojo,
    .mojoSharedBufferHandle,
    .mojoDataPipeProducerHandle,
    .mojoDataPipeConsumerHandle,
    .mojoCreateSharedBufferResult,
    .mojoCreateDataPipeResult,
    .mojoWriteDataResult,
    .mojoReadDataResult,

    // mojoBase
    .mojoBaseMojomBigBufferSharedMemoryRegion,
    .mojoBaseMojomBigString16,
    .mojoBaseMojomString16,
    .mojoBaseMojomUint128,

    // skia
    .skiaMojomBitmapN32ImageInfo,

    //url
    .urlMojomSchemeHostPort,
    .urlMojomUrl,
]

public let commonMojoEnumerations: [ILType] = []

public let commonMojoOptionsBags: [OptionsBag] = [
    // mojo
    .mojoCreateDataPipeOptions,
    .mojoWriteDataOptions,
    .mojoReadDataOptions,
    .mojoDiscardDataOptions,

    // mojoBase
    .mojoBaseMojomBigBuffer,
]

extension ILType {
    // mojo
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
    public static let jsSharedBufferHandle: ILType = .object(
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

    // TODO(crbug.com/554102710): Upstream `jsBufferSource` to be a builtin type in Fuzzilli
    public static let jsBufferSource: ILType =
        JavaScriptEnvironment.typedArrayConstructors
        .map { jsTypedArray($0) }
        .reduce(.jsArrayBuffer | .jsDataView, |)

    // These type are used to create a parameterized `jsArray`s, since the type
    // argument provided to `createJsArrayType` needs to have a group. Note
    // that Fuzzilli will evaluate that primitive variables may be of these
    // proxy types (e.g. `integer` variables may be (`ILType.MayBe`) of type
    // `int16Element`)
    public static let jsBoolElement: ILType =
        .boolean + .object(ofGroup: CommonMojoStrings.boolElement)
    public static let jsInt8Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.int8Element)
    public static let jsInt16Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.int16Element)
    public static let jsInt32Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.int32Element)
    public static let jsInt64Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.int64Element)
    public static let jsUint8Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.uint8Element)
    public static let jsUint16Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.uint16Element)
    public static let jsUint32Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.uint32Element)
    public static let jsUint64Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.uint64Element)
    public static let jsFloatElement: ILType =
        .float + .object(ofGroup: CommonMojoStrings.floatElement)
    public static let jsStringElement: ILType =
        .string + .object(ofGroup: CommonMojoStrings.stringElement)

    // mojoBase
    public static let jsMojoBaseMojomBigBuffer: ILType = OptionsBag.mojoBaseMojomBigBuffer.group
        .instanceType

    public static let jsMojoBaseMojomBigBufferSharedMemoryRegion: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion,
        withProperties: ["bufferHandle", "size"])
    public static let jsMojoBaseMojomBigBufferSharedMemoryRegionConstructor: ILType = .constructor(
        [.plain(.jsSharedBufferHandle), .integer] => .jsMojoBaseMojomBigBufferSharedMemoryRegion
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
        [.plain(.createJsArrayType(ofElementType: .jsInt16Element))] => .jsMojoBaseMojomString16
    )
    public static let jsMojoBaseMojomUint128: ILType = .object(
        ofGroup: CommonMojoStrings.mojoBaseMojomUint128, withProperties: ["high", "low"])

    // skia
    // TODO(crbug.com/546113480): Remove BitmapN32ImageInfo once fixed-size array generation
    // logic is supported, as the Fuzzilli profile generator can then generate this type.
    public static let jsSkiaMojomBitmapN32ImageInfo: ILType = .object(
        ofGroup: CommonMojoStrings.skiaMojomBitmapN32ImageInfo,
        withProperties: [
            "alphaType", "width", "height", "colorTransferFunction", "colorToXyzMatrix",
        ]
    )
    public static let jsSkiaMojomBitmapN32ImageInfoConstructor: ILType = .constructor(
        [
            .plain(.jsSkiaMojomAlphaType), .integer, .integer,
            .plain(.jsSkiaMojomColorTransferFunctionArray),
            .plain(.jsSkiaMojomColorToXyzMatrixArray),
        ] => .jsSkiaMojomBitmapN32ImageInfo
    )
    public static let jsSkiaMojomAlphaType: ILType = .intEnumeration(
        ofName: CommonMojoStrings.skiaMojomAlphaType, withValues: Array(0...4)
    )
    public static let jsSkiaMojomColorTransferFunctionArray: ILType = .createJsArrayType(
        ofElementType: .jsFloatElement)
    public static let jsSkiaMojomColorToXyzMatrixArray: ILType = .createJsArrayType(
        ofElementType: .jsFloatElement)

    // url
    public static let jsUrlMojomUrl: ILType = .object(
        ofGroup: CommonMojoStrings.urlMojomUrl, withProperties: ["url"])
    public static let jsUrlMojomUrlConstructor: ILType = .constructor(
        [.string] => .jsUrlMojomUrl
    )
    public static let jsUrlMojomSchemeHostPort: ILType = .object(
        ofGroup: CommonMojoStrings.urlMojomSchemeHostPort,
        withProperties: ["scheme", "host", "port"])
    public static let jsUrlMojomSchemeHostPortConstructor: ILType = .constructor(
        [.string, .string, .integer] => .jsUrlMojomSchemeHostPort
    )
}

extension ObjectGroup {
    // mojo
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
        instanceType: .jsSharedBufferHandle,
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
            "handle": .jsSharedBufferHandle,
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

    public static let boolElement = ObjectGroup(
        name: CommonMojoStrings.boolElement,
        instanceType: .jsBoolElement,
        properties: [:],
        methods: [:]
    )
    public static let int8Element = ObjectGroup(
        name: CommonMojoStrings.int8Element,
        instanceType: .jsInt8Element,
        properties: [:],
        methods: [:]
    )
    public static let int16Element = ObjectGroup(
        name: CommonMojoStrings.int16Element,
        instanceType: .jsInt16Element,
        properties: [:],
        methods: [:]
    )
    public static let int32Element = ObjectGroup(
        name: CommonMojoStrings.int32Element,
        instanceType: .jsInt32Element,
        properties: [:],
        methods: [:]
    )
    public static let int64Element = ObjectGroup(
        name: CommonMojoStrings.int64Element,
        instanceType: .jsInt64Element,
        properties: [:],
        methods: [:]
    )
    public static let uint8Element = ObjectGroup(
        name: CommonMojoStrings.uint8Element,
        instanceType: .jsUint8Element,
        properties: [:],
        methods: [:]
    )
    public static let uint16Element = ObjectGroup(
        name: CommonMojoStrings.uint16Element,
        instanceType: .jsUint16Element,
        properties: [:],
        methods: [:]
    )
    public static let uint32Element = ObjectGroup(
        name: CommonMojoStrings.uint32Element,
        instanceType: .jsUint32Element,
        properties: [:],
        methods: [:]
    )
    public static let uint64Element = ObjectGroup(
        name: CommonMojoStrings.uint64Element,
        instanceType: .jsUint64Element,
        properties: [:],
        methods: [:]
    )
    public static let floatElement = ObjectGroup(
        name: CommonMojoStrings.floatElement,
        instanceType: .jsFloatElement,
        properties: [:],
        methods: [:]
    )
    public static let stringElement = ObjectGroup(
        name: CommonMojoStrings.stringElement,
        instanceType: .jsStringElement,
        properties: [:],
        methods: [:]
    )

    // mojoBase
    public static let mojoBaseMojomBigBufferSharedMemoryRegion = ObjectGroup(
        name: CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion,
        instanceType: .jsMojoBaseMojomBigBufferSharedMemoryRegion,
        properties: [
            "bufferHandle": .jsSharedBufferHandle,
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
            "data": .createJsArrayType(ofElementType: .jsInt16Element)
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

    // skia
    public static let skiaMojomBitmapN32ImageInfo = ObjectGroup(
        name: CommonMojoStrings.skiaMojomBitmapN32ImageInfo,
        instanceType: .jsSkiaMojomBitmapN32ImageInfo,
        properties: [
            "alphaType": .jsSkiaMojomAlphaType,
            "width": .integer,
            "height": .integer,
            "colorTransferFunction": .jsSkiaMojomColorTransferFunctionArray,
            "colorToXyzMatrix": .jsSkiaMojomColorToXyzMatrixArray,
        ],
        methods: [:]
    )

    // url
    public static let urlMojomUrl = ObjectGroup(
        name: CommonMojoStrings.urlMojomUrl,
        instanceType: .jsUrlMojomUrl,
        properties: [
            "url": .string
        ],
        methods: [:]
    )
    public static let urlMojomSchemeHostPort = ObjectGroup(
        name: CommonMojoStrings.urlMojomSchemeHostPort,
        instanceType: .jsUrlMojomSchemeHostPort,
        properties: [
            "scheme": .string,
            "host": .string,
            "port": .integer,
        ],
        methods: [:]
    )
}

extension OptionsBag {
    // mojo
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

    // mojoBase
    public static let mojoBaseMojomBigBuffer = OptionsBag(
        name: CommonMojoStrings.mojoBaseMojomBigBuffer,
        properties: [
            "bytes": .createJsArrayType(ofElementType: .jsUint8Element),
            "sharedMemory": ILType.jsMojoBaseMojomBigBufferSharedMemoryRegion,
            "invalidBuffer": .boolean,
        ],
        selectionMode: .exactlyOne
    )
}

// mojo
/// A union in the Mojo JavaScript bindings is represented as an object literal
/// with exactly one key-value pair, where the key is the union variant and the
/// value is the variant's value (for example, in `big_buffer.mojom`, a
/// BigBuffer holding a boolean is represented as `{"invalid_buffer": true}`).
///
/// Fuzzilli mutators randomly select instructions and variables to mutate. When
/// an object literal is selected, Fuzzilli searches for a `CodeGenerator`
/// registered for the `.objectLiteral` context. The builtin generators in this
/// context modify properties or methods. Modifying an object literal that
/// represents a union invalidates the union type instantiation, producing
/// unhelpful mutations that are immediately rejected by the C++ validation
/// layer.
///
/// However, `.objectLiteral` generators cannot simply be disabled: Fuzzilli
/// crashes if it fails to find an applicable generator for an active context.
/// Providing this no-op generator ensures Fuzzilli finds a valid generator to
/// run without mutating and invalidating the union object literals. All the
/// other `.objectLiteral` CodeGenerators are disabled.
public let MojoObjectLiteralNoopGenerator = CodeGenerator(
    "MojoObjectLiteralNoopGenerator",
    inContext: .single(.objectLiteral)
) { b in }

/// CodeGenerator producing a BufferSource (ArrayBuffer, DataView, or any TypedArray).
public let MojoBufferSourceGenerator = CodeGenerator(
    "MojoBufferSourceGenerator",
    inputs: .one,
    produces: [.jsBufferSource]
) { b, _ in
    enum bufferSourceKind: CaseIterable {
        case arrayBuffer
        case dataView
        case typedArray
    }

    switch chooseUniform(from: bufferSourceKind.allCases) {
    case .arrayBuffer:
        let _ = b.findOrGenerateType(.jsArrayBuffer)

    case .dataView:
        let _ = b.findOrGenerateType(.jsDataView)

    case .typedArray:
        let variant = chooseUniform(from: JavaScriptEnvironment.typedArrayConstructors)
        let _ = b.findOrGenerateType(ILType.jsTypedArray(variant))
    }
}

// mojoBase
public let MojoMojoBaseMojomBigBufferBytesGenerator = CodeGenerator(
    "MojoMojoBaseMojomBigBufferBytesGenerator",
    inputs: .one,
    produces: [.createJsArrayType(ofElementType: .jsUint8Element)]
) { b, _ in
    var elements: [Variable] = [b.loadInt(Int64.random(in: 0...255))]
    b.createArray(with: elements, elementGroupName: CommonMojoStrings.uint8Element)
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
    produces: [.jsMojoBaseMojomString16]
) { b in
    // Ideally the CodeGenerator would use a string from the JavaScript
    // program. Such a string would be represented by a `Variable` object, and
    // there is no clean way to grab the underlying string value from a
    // `Variable` object. So, instead, generate a random Swift string.
    let randomStr = b.randomString()
    var elements: [Variable] = []
    // Convert the string into a jsArray of integer variables
    for charCode in randomStr.utf16 {
        elements.append(b.loadInt(Int64(charCode)))
    }
    let array = b.createArray(with: elements, elementGroupName: CommonMojoStrings.int16Element)
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.mojoBaseMojomString16)
    b.construct(constructor, withArgs: [array])
}

// skia
public let MojoSkiaMojomColorTransferFunctionArrayGenerator = CodeGenerator(
    "MojoSkiaMojomColorTransferFunctionArrayGenerator",
    inputs: .one,
    produces: [.jsSkiaMojomColorTransferFunctionArray],
) { b, _ in
    var floats: [Variable] = []
    for _ in 1...7 {
        floats.append(b.loadFloat(b.randomFloat()))
    }
    b.createArray(with: floats, elementGroupName: CommonMojoStrings.floatElement)
}

public let MojoSkiaMojomColorToXyzMatrixArrayGenerator = CodeGenerator(
    "MojoSkiaMojomColorToXyzMatrixArrayGenerator",
    inputs: .one,
    produces: [.jsSkiaMojomColorToXyzMatrixArray],
) { b, _ in
    var floats: [Variable] = []
    for _ in 1...9 {
        floats.append(b.loadFloat(b.randomFloat()))
    }
    b.createArray(with: floats, elementGroupName: CommonMojoStrings.floatElement)
}

// url
// TODO(http://crbug.com/514397167) determine broader URL generation strategy
public let MojoUrlMojomUrlGenerator = CodeGenerator(
    "MojoUrlMojomUrlGenerator",
    produces: [.jsUrlMojomUrl]
) { b in
    let urlString = b.loadString("https://example.com/" + b.randomString())
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.urlMojomUrl)
    b.construct(constructor, withArgs: [urlString])
}

// TODO(http://crbug.com/514397167) determine broader SchemeHostPort generation strategy
public let MojoUrlMojomUrlSchemeHostPortGenerator = CodeGenerator(
    "MojoUrlMojomUrlSchemeHostPortGenerator",
    produces: [.jsUrlMojomSchemeHostPort]
) { b in
    let schemes = ["https", "wss"]
    let hosts = ["example.com", "localhost", "127.0.0.1", "[::1]", "xn--n3h.net"]

    let selectedScheme = chooseUniform(from: schemes)
    let selectedHost = chooseUniform(from: hosts)

    let port = Int64.random(in: 1...65535)

    let schemeVar = b.loadString(selectedScheme)
    let hostVar = b.loadString(selectedHost)
    let portVar = b.loadInt(port)

    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.urlMojomSchemeHostPort)
    b.construct(constructor, withArgs: [schemeVar, hostVar, portVar])
}
