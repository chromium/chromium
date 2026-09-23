// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// The files in this `common/` directory contain public definitions of
/// non-interface specific Mojo types, constructors, and CodeGenerators. They do
/// not provide a standalone profile that can be used for fuzzing (i.e.,
/// `--profile=mojoCommon` is invalid).
///
/// Types defined in these files are only used by generated profiles if they are
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
/// 6. Module lists (of ObjectGroups, CodeGenerators, etc.):
///    - Namespace concatenated with the registry kind, in camelCase (e.g.
///      `urlMojomObjectGroups`, `mojoBaseMojomCodeGenerators`).

public enum CommonMojoStrings {
    static let boolElement = "Bool"
    // These integer names match the names of `namedIntegers` defined in
    // `JavaScriptEnvironment`. Type merging requires that the two types share
    // the same `group`.
    static let int8Element = "Int8"
    static let int16Element = "Int16"
    static let int32Element = "Int32"
    static let int64Element = "Int64"
    static let uint8Element = "Uint8"
    static let uint16Element = "Uint16"
    static let uint32Element = "Uint32"
    static let uint64Element = "Uint64"
    static let floatElement = "Float"
    // There exists already an `ObjectGroup` with the `group` "String"
    static let stringElement = "StringElement"
}

public let commonMojoBuiltins: [String: ILType] =
    mojoCoreBuiltins
    .merging(mojoBaseMojomBuiltins) { (existing, _) in existing }
    .merging(skiaMojomBuiltins) { (existing, _) in existing }
    .merging(urlMojomBuiltins) { (existing, _) in existing }

public let commonMojoCodeGenerators: [(CodeGenerator, Int)] =
    sharedMojoCodeGenerators
    + mojoBaseMojomCodeGenerators
    + skiaMojomCodeGenerators
    + urlMojomCodeGenerators

public let commonMojoObjectGroups: [ObjectGroup] =
    sharedMojoObjectGroups
    + mojoCoreObjectGroups
    + mojoBaseMojomObjectGroups
    + skiaMojomObjectGroups
    + urlMojomObjectGroups

public let commonMojoEnumerations: [ILType] =
    networkMojomEnumerations
    + skiaMojomEnumerations

public let commonMojoOptionsBags: [OptionsBag] =
    mojoCoreOptionsBags
    + mojoBaseMojomOptionsBags

public let sharedMojoCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoObjectLiteralNoopGenerator, 1),
    (MojoBufferSourceGenerator, 1),
]

public let sharedMojoObjectGroups: [ObjectGroup] = [
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
]

extension ILType {
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
        .jsInt8 + .object(ofGroup: CommonMojoStrings.int8Element)
    public static let jsInt16Element: ILType =
        .jsInt16 + .object(ofGroup: CommonMojoStrings.int16Element)
    public static let jsInt32Element: ILType =
        .jsInt32 + .object(ofGroup: CommonMojoStrings.int32Element)
    public static let jsInt64Element: ILType =
        .bigint + .object(ofGroup: CommonMojoStrings.int64Element)
    public static let jsUint8Element: ILType =
        .jsUint8 + .object(ofGroup: CommonMojoStrings.uint8Element)
    public static let jsUint16Element: ILType =
        .jsUint16 + .object(ofGroup: CommonMojoStrings.uint16Element)
    public static let jsUint32Element: ILType =
        .jsUint32 + .object(ofGroup: CommonMojoStrings.uint32Element)
    // TODO(crbug.com/553587894): Determine a way to support uint64
    public static let jsUint64Element: ILType =
        .bigint + .object(ofGroup: CommonMojoStrings.uint64Element)
    public static let jsFloatElement: ILType =
        .float + .object(ofGroup: CommonMojoStrings.floatElement)
    public static let jsStringElement: ILType =
        .string + .object(ofGroup: CommonMojoStrings.stringElement)
}

extension ObjectGroup {
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
}

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
