// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// This file contains public definitions of non-interface specific Mojo types,
/// constructors, and CodeGenerators. It does not provide a standalone profile
/// that can be used for fuzzing (i.e., `--profile=mojoCommon` is invalid).

public enum CommonMojoStrings {
    // mojoBase
    static let string16 = "mojoBase.mojom.String16"
    static let int16Element = "Int16Element"

    // url
    static let url = "url.mojom.Url"
    static let schemeHostPort = "url.mojom.SchemeHostPort"
}

public let commonMojoBuiltins: [String: ILType] = [
    // mojoBase
    CommonMojoStrings.string16: .jsString16Constructor,

    //url
    CommonMojoStrings.schemeHostPort: .jsSchemeHostPortConstructor,
    CommonMojoStrings.url: .jsUrlConstructor,
]

public let commonMojoCodeGenerators: [(CodeGenerator, Int)] = [
    // mojo
    (MojoObjectLiteralNoopGenerator, 1),

    // mojoBase
    (MojoString16Generator, 1),

    // url
    (MojoSchemeHostPortGenerator, 1),
    (MojoUrlGenerator, 1),
]

public let commonMojoObjectGroups: [ObjectGroup] = [
    // mojoBase
    .int16Element,
    .string16,

    //url
    .schemeHostPort,
    .url,
]

public let commonMojoEnumerations: [ILType] = []

public let commonMojoOptionsBags: [OptionsBag] = []

extension ILType {
    public static let jsString16: ILType = .object(
        ofGroup: CommonMojoStrings.string16, withProperties: ["data"])
    public static let jsString16Constructor: ILType = .constructor(
        [.plain(.createJsArrayType(ofElementType: .int16Element))] => .jsString16
    )

    // This type is used to create a parameterized jsArray, since the type
    // argument provided to `createJsArrayType` needs to have a group. Note
    // that Fuzzilli will evaluate that `integer` objects "may be"
    // (`ILType.MayBe`) of type `int16Element`
    public static let int16Element: ILType =
        .integer + .object(ofGroup: CommonMojoStrings.int16Element)

    // url
    public static let jsUrl: ILType = .object(
        ofGroup: CommonMojoStrings.url, withProperties: ["url"])
    public static let jsUrlConstructor: ILType = .constructor(
        [.string] => .jsUrl
    )
    public static let jsSchemeHostPort: ILType = .object(
        ofGroup: CommonMojoStrings.schemeHostPort,
        withProperties: ["scheme", "host", "port"])
    public static let jsSchemeHostPortConstructor: ILType = .constructor(
        [.string, .string, .integer] => .jsSchemeHostPort
    )
}
extension ObjectGroup {
    public static let string16 = ObjectGroup(
        name: CommonMojoStrings.string16,
        instanceType: .jsString16,
        properties: [
            "data": .createJsArrayType(ofElementType: .int16Element)
        ],
        methods: [:]
    )

    public static let int16Element = ObjectGroup(
        name: CommonMojoStrings.int16Element,
        instanceType: .int16Element,
        properties: [:],
        methods: [:]
    )

    // url
    public static let url = ObjectGroup(
        name: CommonMojoStrings.url,
        instanceType: .jsUrl,
        properties: [
            "url": .string
        ],
        methods: [:]
    )

    public static let schemeHostPort = ObjectGroup(
        name: CommonMojoStrings.schemeHostPort,
        instanceType: .jsSchemeHostPort,
        properties: [
            "scheme": .string,
            "host": .string,
            "port": .integer,
        ],
        methods: [:]
    )
}

//mojo
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

// mojoBase
public let MojoString16Generator = CodeGenerator(
    "MojoString16Generator",
    produces: [.jsString16]
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
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.string16)
    b.construct(constructor, withArgs: [array])
}

// url
// TODO(http://crbug.com/514397167) determine broader URL generation strategy
public let MojoUrlGenerator = CodeGenerator(
    "MojoUrlGenerator",
    produces: [.jsUrl]
) { b in
    let urlString = b.loadString("https://example.com/" + b.randomString())
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.url)
    b.construct(constructor, withArgs: [urlString])
}

// TODO(http://crbug.com/514397167) determine broader SchemeHostPort generation strategy
public let MojoSchemeHostPortGenerator = CodeGenerator(
    "MojoSchemeHostPortGenerator",
    produces: [.jsSchemeHostPort]
) { b in
    let schemes = ["https", "wss"]
    let hosts = ["example.com", "localhost", "127.0.0.1", "[::1]", "xn--n3h.net"]

    let selectedScheme = chooseUniform(from: schemes)
    let selectedHost = chooseUniform(from: hosts)

    let port = Int64.random(in: 1...65535)

    let schemeVar = b.loadString(selectedScheme)
    let hostVar = b.loadString(selectedHost)
    let portVar = b.loadInt(port)

    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.schemeHostPort)
    b.construct(constructor, withArgs: [schemeVar, hostVar, portVar])
}
