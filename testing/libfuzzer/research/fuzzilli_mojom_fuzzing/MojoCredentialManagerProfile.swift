// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

private enum MojoStrings {
    static let credentialManager = "blink.mojom.CredentialManager"
    static let credentialManagerRemote = "blink.mojom.CredentialManagerRemote"
    static let credentialManagerRemoteWrapper = "CredentialManagerRemoteWrapper"
    static let credentialType = "blink.mojom.CredentialType"
    static let credentialMediationRequirement = "blink.mojom.CredentialMediationRequirement"
    static let credentialInfo = "blink.mojom.CredentialInfo"
    static let mojoPrefix = "blink.mojom."
}

extension ILType {
    // CredentialManager
    fileprivate static let jsCredentialManager: ILType = .object(
        ofGroup: MojoStrings.credentialManager,
        withProperties: ["$interfaceName"],
        withMethods: ["getRemote"])
    fileprivate static let jsCredentialManagerRemote: ILType = .object(
        ofGroup: MojoStrings.credentialManagerRemote, withProperties: ["$"],
        withMethods: ["preventSilentAccess", "store", "get"])
    fileprivate static let jsCredentialManagerRemoteWrapper: ILType = .object(
        ofGroup: MojoStrings.credentialManagerRemoteWrapper,
        withMethods: ["close", "isBound"])

    // Data types
    fileprivate static let jsCredentialType: ILType = .intEnumeration(
        ofName: MojoStrings.credentialType, withValues: Array(0...2))
    fileprivate static let jsCredentialMediationRequirement: ILType =
        .intEnumeration(
            ofName: MojoStrings.credentialMediationRequirement,
            withValues: Array(0...3))

    fileprivate static let jsUrlArray: ILType = .createJsArrayType(ofElementType: .jsUrl)
    fileprivate static let jsCredentialInfo: ILType = .object(
        ofGroup: MojoStrings.credentialInfo,
        withProperties: ["type", "id", "name", "icon", "password", "federation"])
}

private let credentialManager = ObjectGroup(
    name: MojoStrings.credentialManager,
    instanceType: .jsCredentialManager,
    properties: [
        "$interfaceName": .string
    ],
    methods: [
        "getRemote": [] => .jsCredentialManagerRemote
    ]
)

private let credentialManagerRemote = ObjectGroup(
    name: MojoStrings.credentialManagerRemote,
    instanceType: .jsCredentialManagerRemote,
    properties: [
        "$": .jsCredentialManagerRemoteWrapper
    ],
    methods: [
        "preventSilentAccess": [] => .jsPromise,
        "store": [.plain(.jsCredentialInfo)] => .jsPromise,
        "get": [
            .plain(.jsCredentialMediationRequirement), .boolean, .plain(.jsUrlArray),
        ] => .jsPromise,
    ]
)

private let credentialManagerRemoteWrapper = ObjectGroup(
    name: MojoStrings.credentialManagerRemoteWrapper,
    instanceType: .jsCredentialManagerRemoteWrapper,
    properties: [:],
    methods: [
        "close": [] => .undefined,
        "isBound": [] => .boolean,
    ]
)

private let credentialInfo = ObjectGroup(
    name: MojoStrings.credentialInfo,
    instanceType: .jsCredentialInfo,
    properties: [
        "type": .jsCredentialType,
        "id": .jsString16,
        "name": .jsString16,
        "icon": .jsUrl,
        "password": .jsString16,
        "federation": .jsSchemeHostPort,
    ],
    methods: [:]
)

private let mojoBuiltins: [String: ILType] = [
    MojoStrings.credentialManager: .jsCredentialManager,
    CommonMojoStrings.string16: .jsString16Constructor,
    CommonMojoStrings.url: .jsUrlConstructor,
    CommonMojoStrings.schemeHostPort: .jsSchemeHostPortConstructor,
    MojoStrings.credentialInfo: .constructor(
        [
            .plain(.jsCredentialType),
            .plain(.jsString16),
            .plain(.jsString16),
            .plain(.jsUrl),
            .plain(.jsString16),
            .plain(.jsSchemeHostPort),
        ]
            => .jsCredentialInfo
    ),
]

// Program Template to force Mojo usage
private let MojoCredentialManagerFuzzer = ProgramTemplate(
    "MojoCredentialManagerFuzzer"
) { b in
    b.buildPrefix()

    // Get the CredentialManager remote
    let managerStatic = b.createNamedVariable(
        forBuiltin: MojoStrings.credentialManager)
    b.callMethod("getRemote", on: managerStatic, withArgs: [])

    // Generate random code to use the objects further
    b.build(n: 50)
}

private func isTargetObject(type: ILType) -> Bool {
    guard type.Is(.object()), let group = type.group else { return false }
    return group.starts(with: MojoStrings.mojoPrefix)
        || group.starts(with: "mojoBase.mojom.") || group.starts(with: "url.mojom.")
}

/// Mojo variant of the builtin `MethodCallGenerator` that operates only on
/// "Mojo" objects instead of all the objects in the JavaScript environment,
/// most of which are unrelated to the interface. This generator emits random
/// method calls, generating any arguments that are not available.
private let MojoMethodCallGenerator = CodeGenerator("MojoMethodCallGenerator") {
    b in
    // Pick a variable among all Mojo variables with methods
    var targetVar = b.findVariable {
        let type = b.type(of: $0)
        return isTargetObject(type: type) && !type.methods.isEmpty
            && type.group != MojoStrings.credentialManager
    }

    // If we can't find a Mojo object in scope, create the credential manager remote
    guard let obj = targetVar else {
        let managerStatic = b.createNamedVariable(
            forBuiltin: MojoStrings.credentialManager)
        b.callMethod("getRemote", on: managerStatic, withArgs: [])
        return
    }

    let methodName = b.type(of: obj).randomMethod()!

    let signatures = b.methodSignatures(of: methodName, on: obj)
    let signature = chooseUniform(from: signatures)
    let arguments = b.findOrGenerateArguments(forSignature: signature)

    b.callMethod(methodName, on: obj, withArgs: arguments, guard: false)
}

/// Mojo variant of the builtin `PropertyRetrievalGenerator` that operates only
/// on "Mojo" objects instead of all the objects in the JavaScript environment,
/// most of which are unrelated to the interface. This generator emits random
/// property retrievals.
private let MojoPropertyRetrievalGenerator = CodeGenerator(
    "MojoPropertyRetrievalGenerator"
) { b in
    let targetVars = b.visibleVariables.filter {
        isTargetObject(type: b.type(of: $0))
    }
    guard !targetVars.isEmpty else { return }
    let obj = chooseUniform(from: targetVars)

    let propertyName =
        b.type(of: obj).randomProperty() ?? b.randomCustomPropertyName()
    b.getProperty(propertyName, of: obj)
}

private let MojoUrlArrayGenerator = CodeGenerator(
    "MojoUrlArrayGenerator", inputs: .required(.jsUrl), produces: [.jsUrlArray]
) {
    b, url in
    b.createArray(with: [url], elementGroupName: ObjectGroup.urlGroup.name)
}

private let keepGenerators = [
    "IntegerGenerator", "StringGenerator", "BooleanGenerator", "TernaryOperationGenerator",
    "PlainFunctionGenerator", "SubroutineReturnGenerator", "AsyncFunctionGenerator",
    "TryCatchGenerator", "AwaitGenerator",
]
private let mojoDisabledGenerators: [String] = CodeGenerators.map { $0.name }
    .filter {
        !keepGenerators.contains($0)
    }

let mojoCredentialManagerProfile = Profile(
    processArgs: { _ in return [] },
    processArgsReference: nil,
    processEnv: [
        "ASAN_OPTIONS": "detect_odr_violation=0:abort_on_error=1", "DISPLAY": ":20",
    ],
    maxExecsBeforeRespawn: 1000,
    timeout: Timeout.interval(11000, 11000),
    codePrefix: "",
    codeSuffix: "",
    ecmaVersion: v8Profile.ecmaVersion,
    startupTests: [
        ("fuzzilli('FUZZILLI_PRINT', 'test')", .shouldSucceed),
        (
            "if (typeof blink === 'undefined' || typeof blink.mojom === 'undefined' || typeof \(MojoStrings.credentialManager) === 'undefined') throw 'CredentialManager not found'",
            .shouldSucceed
        ),
    ] + v8Profile.startupTests,
    additionalCodeGenerators: [
        (MojoMethodCallGenerator, 70),
        (MojoPropertyRetrievalGenerator, 50),
        (MojoString16Generator, 5),
        (MojoUrlGenerator, 5),
        (MojoUrlArrayGenerator, 5),
        (MojoSchemeHostPortGenerator, 5),
    ],
    additionalProgramTemplates: WeightedList([
        // Heavily bias Fuzzilli to use the ProgramTemplate that establishes a Mojo connection.
        (MojoCredentialManagerFuzzer, 1000)
    ]),
    disabledCodeGenerators: mojoDisabledGenerators,
    disabledMutators: v8Profile.disabledMutators,
    additionalBuiltins: mojoBuiltins,
    additionalObjectGroups: [
        credentialManager,
        credentialManagerRemote,
        credentialManagerRemoteWrapper,
        ObjectGroup.int16Element,
        ObjectGroup.string16,
        ObjectGroup.urlGroup,
        ObjectGroup.schemeHostPort,
        credentialInfo,
    ],
    additionalEnumerations: [
        .jsCredentialType, .jsCredentialMediationRequirement,
    ],
    additionalOptionsBags: [],
    optionalPostProcessor: nil
)
