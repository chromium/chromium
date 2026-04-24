// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

private enum MojoStrings {
    static let lockManager = "blink.mojom.LockManager"
    static let lockManagerRemote = "blink.mojom.LockManagerRemote"
    static let lockManagerRemoteWrapper = "LockManagerRemoteWrapper"
    static let lockRequestGrantedCallbackReceiver =
        "LockRequestGrantedCallbackReceiver"
    static let lockRequestCallbackRouterReceiverHelper =
        "LockRequestCallbackRouterReceiverHelper"
    static let lockRequestFailedCallbackReceiver =
        "LockRequestFailedCallbackReceiver"
    static let lockRequestCallbackRouter = "blink.mojom.LockRequestCallbackRouter"
    static let lockRequestRemote = "blink.mojom.LockRequestRemote"
    static let lockHandleCallbackRouter = "blink.mojom.LockHandleCallbackRouter"
    static let lockHandleCallbackRouterReceiverHelper =
        "LockHandleCallbackRouterReceiverHelper"
    static let lockHandleRemote = "blink.mojom.LockHandleRemote"
    static let lockHandleRemoteWrapper = "LockHandleRemoteWrapper"
    static let lockMode = "blink.mojom.LockMode"
    static let waitMode = "blink.mojom.LockManager.WaitMode"
    static let lockInfo = "LockInfo"
    static let mojoPrefix = "blink.mojom."
}

extension ILType {
    // LockManager
    fileprivate static let jsLockManager: ILType = .object(
        ofGroup: MojoStrings.lockManager,
        withProperties: ["$interfaceName"],
        withMethods: ["getRemote"])
    fileprivate static let jsLockManagerRemote: ILType = .object(
        ofGroup: MojoStrings.lockManagerRemote,
        withProperties: ["$"],
        withMethods: ["requestLock", "queryState"])
    fileprivate static let jsLockManagerRemoteWrapper: ILType = .object(
        ofGroup: MojoStrings.lockManagerRemoteWrapper,
        withMethods: ["close"])

    // LockRequest
    fileprivate static let jsLockRequestCallbackRouter: ILType = .object(
        ofGroup: MojoStrings.lockRequestCallbackRouter,
        withProperties: [
            "$", "granted", "failed",
        ])
    fileprivate static let jsLockRequestCallbackRouterReceiverHelper: ILType =
        .object(
            ofGroup: MojoStrings.lockRequestCallbackRouterReceiverHelper,
            withMethods: ["associateAndPassRemote", "closeBindings"])
    fileprivate static let jsLockRequestRemote: ILType = .object(
        ofGroup: MojoStrings.lockRequestRemote)

    // Callback Receivers for LockRequest
    fileprivate static let jsLockRequestGrantedCallbackReceiver: ILType = .object(
        ofGroup: MojoStrings.lockRequestGrantedCallbackReceiver,
        withMethods: ["addListener"])
    fileprivate static let jsLockRequestFailedCallbackReceiver: ILType = .object(
        ofGroup: MojoStrings.lockRequestFailedCallbackReceiver,
        withMethods: ["addListener"])

    // LockHandle
    fileprivate static let jsLockHandleCallbackRouter: ILType = .object(
        ofGroup: MojoStrings.lockHandleCallbackRouter,
        withProperties: ["$"])
    fileprivate static let jsLockHandleCallbackRouterReceiverHelper: ILType =
        .object(
            ofGroup: MojoStrings.lockHandleCallbackRouterReceiverHelper,
            withMethods: [
                "associateAndPassRemote", "closeBindings",
            ])
    fileprivate static let jsLockHandleRemote: ILType = .object(
        ofGroup: MojoStrings.lockHandleRemote,
        withProperties: ["$"])
    fileprivate static let jsLockHandleRemoteWrapper: ILType = .object(
        ofGroup: MojoStrings.lockHandleRemoteWrapper,
        withMethods: ["close"])

    // Data types
    fileprivate static let jsLockMode: ILType = .intEnumeration(
        ofName: MojoStrings.lockMode, withValues: [0, 1])
    fileprivate static let jsWaitMode: ILType = .intEnumeration(
        ofName: MojoStrings.waitMode, withValues: [0, 1, 2])
    fileprivate static let jsLockInfo: ILType = .object(
        ofGroup: MojoStrings.lockInfo,
        withProperties: [
            "name", "mode", "client_id",
        ])
}

private let lockManager = ObjectGroup(
    name: MojoStrings.lockManager,
    instanceType: .jsLockManager,
    properties: [
        "$interfaceName": .string
    ],
    methods: [
        "getRemote": [] => .jsLockManagerRemote
    ]
)

private let lockManagerRemote = ObjectGroup(
    name: MojoStrings.lockManagerRemote,
    instanceType: .jsLockManagerRemote,
    properties: [
        "$": .jsLockManagerRemoteWrapper
    ],
    methods: [
        "requestLock": [
            .string, .plain(.jsLockMode), .plain(.jsWaitMode),
            .plain(.jsLockRequestRemote),
        ] => .undefined,
        "queryState": [] => .jsPromise,  // Returns {requested: [LockInfo], held: [LockInfo]}
    ]
)

private let lockManagerRemoteWrapper = ObjectGroup(
    name: MojoStrings.lockManagerRemoteWrapper,
    instanceType: .jsLockManagerRemoteWrapper,
    properties: [:],
    methods: [
        "close": [] => .undefined
    ]
)

private let lockRequestCallbackRouter = ObjectGroup(
    name: MojoStrings.lockRequestCallbackRouter,
    instanceType: .jsLockRequestCallbackRouter,
    properties: [
        "$": .jsLockRequestCallbackRouterReceiverHelper,
        "granted": .jsLockRequestGrantedCallbackReceiver,
        "failed": .jsLockRequestFailedCallbackReceiver,
    ],
    methods: [:]
)

private let lockRequestCallbackRouterReceiverHelper = ObjectGroup(
    name: MojoStrings.lockRequestCallbackRouterReceiverHelper,
    instanceType: .jsLockRequestCallbackRouterReceiverHelper,
    properties: [:],
    methods: [
        "associateAndPassRemote": [] => .jsLockRequestRemote,
        "closeBindings": [] => .undefined,
    ]
)

private let lockRequestGrantedCallbackReceiver = ObjectGroup(
    name: MojoStrings.lockRequestGrantedCallbackReceiver,
    instanceType: .jsLockRequestGrantedCallbackReceiver,
    properties: [:],
    methods: [
        "addListener": [
            .plain(.function([.plain(.jsLockHandleRemote)] => .undefined))
        ] => .integer
    ]
)

private let lockRequestFailedCallbackReceiver = ObjectGroup(
    name: MojoStrings.lockRequestFailedCallbackReceiver,
    instanceType: .jsLockRequestFailedCallbackReceiver,
    properties: [:],
    methods: [
        "addListener": [.plain(.function([] => .undefined))] => .integer
    ]
)

private let lockRequestRemote = ObjectGroup(
    name: MojoStrings.lockRequestRemote,
    instanceType: .jsLockRequestRemote,
    properties: [:],
    methods: [:]
)

private let lockHandleCallbackRouter = ObjectGroup(
    name: MojoStrings.lockHandleCallbackRouter,
    instanceType: .jsLockHandleCallbackRouter,
    properties: [
        "$": .jsLockHandleCallbackRouterReceiverHelper
    ],
    methods: [:]
)

private let lockHandleCallbackRouterReceiverHelper = ObjectGroup(
    name: MojoStrings.lockHandleCallbackRouterReceiverHelper,
    instanceType: .jsLockHandleCallbackRouterReceiverHelper,
    properties: [:],
    methods: [
        "associateAndPassRemote": [] => .jsLockHandleRemote,
        "closeBindings": [] => .undefined,
    ]
)

private let lockHandleRemote = ObjectGroup(
    name: MojoStrings.lockHandleRemote,
    instanceType: .jsLockHandleRemote,
    properties: [
        "$": .jsLockHandleRemoteWrapper
    ],
    methods: [:]
)

private let lockHandleRemoteWrapper = ObjectGroup(
    name: MojoStrings.lockHandleRemoteWrapper,
    instanceType: .jsLockHandleRemoteWrapper,
    properties: [:],
    methods: [
        "close": [] => .undefined
    ]
)

private let lockInfo = ObjectGroup(
    name: MojoStrings.lockInfo,
    instanceType: .jsLockInfo,
    properties: [
        "name": .string,
        "mode": .jsLockMode,
        "client_id": .string,
    ],
    methods: [:]
)

private let mojoBuiltins: [String: ILType] = [
    MojoStrings.lockManager: .jsLockManager,
    MojoStrings.lockRequestCallbackRouter: .constructor(
        [] => .jsLockRequestCallbackRouter),
    MojoStrings.lockHandleCallbackRouter: .constructor(
        [] => .jsLockHandleCallbackRouter),
]

// Program Template to force Mojo usage
private let MojoLockManagerFuzzer = ProgramTemplate("MojoLockManagerFuzzer") {
    b in
    b.buildPrefix()

    // Get the LockManager remote
    let managerStatic = b.createNamedVariable(
        forBuiltin: MojoStrings.lockManager)
    let manager = b.callMethod(
        "getRemote", on: managerStatic, withArgs: [])

    // Generate random code to use the objects further
    b.build(n: 20)
}

/// Mojo variant of the builtin `BuiltinGenerator` that operates only on "Mojo"
/// objects instead of all the objects in the JavaScript environment, most of
/// which are unrelated to the interface. This generates emits a variable of a
/// random Mojo type for use by other CodeGenerators
private let MojoBuiltinGenerator = CodeGenerator("MojoBuiltinGenerator") { b in
    let mojoBuiltinNames = Array(mojoBuiltins.keys)
    let randomMojoBuiltin = chooseUniform(from: mojoBuiltinNames)
    b.createNamedVariable(forBuiltin: randomMojoBuiltin)
}

/// Creates a LockRequestCallbackRouter variable and constructs it for use by
/// other CodeGenerators
private let MojoLockRequestCallbackRouterGenerator = CodeGenerator(
    "MojoLockRequestCallbackRouterGenerator",
    produces: [.jsLockRequestCallbackRouter]
) { b in
    let routerBuiltin = b.createNamedVariable(
        forBuiltin: MojoStrings.lockRequestCallbackRouter)
    b.construct(routerBuiltin, withArgs: [])
}

/// Creates a LockManager variable and calls getRemote on it to get a
/// LockManagerRemote for use by other CodeGenerators. Although
/// MojoBuiltinGenerator and MojoMethodCallGenerator can together generate the
/// same code, this dedicated generator increases the frequency of
/// LockManagerRemote objects in programs.
private let MojoLockManagerRemoteGenerator = CodeGenerator(
    "MojoLockManagerRemoteGenerator", produces: [.jsLockManagerRemote]
) { b in
    let managerStatic = b.createNamedVariable(
        forBuiltin: MojoStrings.lockManager)
    b.callMethod("getRemote", on: managerStatic, withArgs: [])
}

/// Produces a LockRequestRemote, either using an existing
/// LockRequestCallbackRouter or generating one on demand, for use by other
/// CodeGenerators. Although existing generators can together generate the same
/// code, this dedicated generator increases the frequency of LockRequestRemote
/// objects in programs.
private let MojoLockRequestRemoteGenerator = CodeGenerator(
    "MojoLockRequestRemoteGenerator",
    inputs: .required(.jsLockRequestCallbackRouter),
    produces: [.jsLockRequestRemote]
) { b, router in
    let helper = b.getProperty("$", of: router)
    b.callMethod("associateAndPassRemote", on: helper, withArgs: [])
}

/// Emits code for adding a callback to a callback router. Since correctly
/// doing so involves many things (a callback, a property retrieval followed by
/// a method call, etc.), this generator greatly increases the frequency of
/// valid addListener invocations.
private let MojoRouterListenerGenerator = CodeGenerator(
    "MojoRouterListenerGenerator",
    inputs: .required(.jsLockRequestCallbackRouter)
) { b, router in
    guard
        let eventName = ILType.jsLockRequestCallbackRouter.properties
            .subtracting(["$"])
            .randomElement()
    else {
        return
    }

    let listenerHost = b.getProperty(eventName, of: router)
    let callback = b.buildPlainFunction(with: .parameters(n: 1)) { args in
        if probability(0.2) {
            b.buildIf(args[0]) {
                let wrapper = b.getProperty("$", of: args[0])
                b.callMethod("close", on: wrapper, withArgs: [])
            }
        }
        b.build(n: 5)
    }
    b.callMethod("addListener", on: listenerHost, withArgs: [callback])
}

// TODO(crbug.com/500386713) Consider using mojoBuiltins or another list instead of regex
private func isTargetObject(type: ILType) -> Bool {
    guard type.Is(.object()), let group = type.group else { return false }
    return group.starts(with: MojoStrings.mojoPrefix)
        || group.starts(with: "Lock")
        || group == "Promise"
}

/// Mojo variant of the builtin `MethodCallGenerator` that operates only on
/// "Mojo" objects instead of all the objects in the JavaScript environment,
/// most of which are unrelated to the interface. This generates emits random
/// method calls, generating any arguments that are not available.
private let MojoMethodCallGenerator = CodeGenerator("MojoMethodCallGenerator") {
    b in
    let targetVar = b.findVariable { isTargetObject(type: b.type(of: $0)) }
    guard let obj = targetVar else { return }

    // ~20% of the time, try to close the object if it has a `$` wrapper.
    // This triggers disconnected channels while requests are queued or held.
    if probability(0.2),
        b.type(of: obj).properties.contains("$")
    {
        let wrapper = b.getProperty("$", of: obj)
        b.callMethod("close", on: wrapper, withArgs: [])
        return
    }

    // Only call known Mojo methods to avoid random JS pollution
    guard let methodName = b.type(of: obj).randomMethod() else { return }

    let signatures = b.methodSignatures(of: methodName, on: obj)
    let signature = chooseUniform(from: signatures)
    let arguments = b.findOrGenerateArguments(forSignature: signature)
    b.callMethod(methodName, on: obj, withArgs: arguments, guard: false)
}

/// Mojo variant of the builtin `PropertyRetrievalGenerator` that operates only
/// on "Mojo" objects instead of all the objects in the JavaScript environment,
/// most of which are unrelated to the interface. This generates emits random
/// property retrievals.
private let MojoPropertyRetrievalGenerator = CodeGenerator(
    "MojoPropertyRetrievalGenerator"
) { b in
    let targetVar = b.findVariable { isTargetObject(type: b.type(of: $0)) }
    guard let obj = targetVar else { return }

    let propertyName =
        b.type(of: obj).randomProperty() ?? b.randomCustomPropertyName()
    b.getProperty(propertyName, of: obj)
}

private let keepGenerators = [
    "IntegerGenerator", "StringGenerator", "TernaryOperationGenerator",
    "PlainFunctionGenerator", "AsyncFunctionGenerator", "AwaitGenerator",
    "SubroutineReturnGenerator", "TryCatchGenerator",
]

private let mojoDisabledGenerators: [String] = CodeGenerators.map { $0.name }
    .filter {
        !keepGenerators.contains($0)
    }

let chromiumMojoProfile = Profile(
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
            "if (typeof blink === 'undefined' || typeof blink.mojom === 'undefined' || typeof \(MojoStrings.lockManager) === 'undefined') throw 'LockManager not found'",
            .shouldSucceed
        ),
    ] + v8Profile.startupTests,
    // TODO(crbug.com/500389756) Investigate automatic generation of weights
    additionalCodeGenerators: [
        (MojoBuiltinGenerator, 100),
        (MojoMethodCallGenerator, 80),
        (MojoPropertyRetrievalGenerator, 80),
        (MojoLockRequestCallbackRouterGenerator, 100),
        (MojoLockManagerRemoteGenerator, 100),
        (MojoLockRequestRemoteGenerator, 100),
        (MojoRouterListenerGenerator, 80),

    ],
    additionalProgramTemplates: WeightedList([
        // Heavily bias Fuzzilli to use the ProgramTemplate that establishes a Mojo connection.
        (MojoLockManagerFuzzer, 1000)
    ]),
    disabledCodeGenerators: mojoDisabledGenerators,
    disabledMutators: v8Profile.disabledMutators,
    additionalBuiltins: mojoBuiltins,
    additionalObjectGroups: [
        lockManager,
        lockManagerRemote,
        lockManagerRemoteWrapper,
        lockRequestCallbackRouter,
        lockRequestCallbackRouterReceiverHelper,
        lockRequestGrantedCallbackReceiver,
        lockRequestFailedCallbackReceiver,
        lockRequestRemote,
        lockHandleCallbackRouter,
        lockHandleCallbackRouterReceiverHelper,
        lockHandleRemote,
        lockHandleRemoteWrapper,
        lockInfo,
    ],
    additionalEnumerations: [
        .jsLockMode, .jsWaitMode,
    ],
    optionalPostProcessor: nil
)
