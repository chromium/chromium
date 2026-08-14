// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

private enum MojoStrings {
    /// Profiles declare this method, a wrapper method around `getRemote` that follows
    /// the Singleton pattern, in `codePrefix`. This method ensures: (1) `getRemote`
    /// is only called once, and (2) the remote is saved to a global variable so it
    /// can be closed at the end of the program.
    static let getOrCreatePrimaryRemote = "getOrCreatePrimaryRemote"

    static let lockManager = "blink.mojom.LockManager"
    static let lockManagerRemote = "blink.mojom.LockManagerRemote"
    static let lockManagerRemoteWrapper = "LockManagerRemoteWrapper"
    static let lockManagerQueryStateResponse = "blink.mojom.LockManagerQueryStateResponse"
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
        withProperties: ["$interfaceName"])
    fileprivate static let jsLockManagerRemote: ILType = .object(
        ofGroup: MojoStrings.lockManagerRemote,
        withProperties: ["$"],
        withMethods: ["requestLock", "queryState"])
    fileprivate static let jsLockManagerRemoteWrapper: ILType = .object(
        ofGroup: MojoStrings.lockManagerRemoteWrapper,
        withMethods: ["close"])
    fileprivate static let jsBlinkMojomLockManagerQueryStateResponse: ILType = .object(
        ofGroup: MojoStrings.lockManagerQueryStateResponse,
        withProperties: ["requested", "held"],
        withMethods:[])

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

extension ObjectGroup {
    fileprivate static let lockManager = ObjectGroup(
        name: MojoStrings.lockManager,
        instanceType: .jsLockManager,
        properties: [
            "$interfaceName": .string
        ],
        methods: [:]
    )

    fileprivate static let lockManagerRemote = ObjectGroup(
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
            "queryState": [] => .jsPromise(resolvingTo: .jsBlinkMojomLockManagerQueryStateResponse),
        ]
    )

    fileprivate static let lockManagerRemoteWrapper = ObjectGroup(
        name: MojoStrings.lockManagerRemoteWrapper,
        instanceType: .jsLockManagerRemoteWrapper,
        properties: [:],
        methods: [
            "close": [] => .undefined
        ]
    )

    fileprivate static let blinkMojomLockManagerQueryStateResponse = ObjectGroup(
        name: "blink.mojom.LockManagerQueryStateResponse",
        instanceType: .jsBlinkMojomLockManagerQueryStateResponse,
        properties: [
            "requested": ILType.createJsArrayType(ofElementType: .jsLockInfo),
            "held": ILType.createJsArrayType(ofElementType: .jsLockInfo),
        ],
        methods: [:]
    )

    fileprivate static let lockRequestCallbackRouter = ObjectGroup(
        name: MojoStrings.lockRequestCallbackRouter,
        instanceType: .jsLockRequestCallbackRouter,
        properties: [
            "$": .jsLockRequestCallbackRouterReceiverHelper,
            "granted": .jsLockRequestGrantedCallbackReceiver,
            "failed": .jsLockRequestFailedCallbackReceiver,
        ],
        methods: [:]
    )

    fileprivate static let lockRequestCallbackRouterReceiverHelper = ObjectGroup(
        name: MojoStrings.lockRequestCallbackRouterReceiverHelper,
        instanceType: .jsLockRequestCallbackRouterReceiverHelper,
        properties: [:],
        methods: [
            "associateAndPassRemote": [] => .jsLockRequestRemote,
            "closeBindings": [] => .undefined,
        ]
    )

    fileprivate static let lockRequestGrantedCallbackReceiver = ObjectGroup(
        name: MojoStrings.lockRequestGrantedCallbackReceiver,
        instanceType: .jsLockRequestGrantedCallbackReceiver,
        properties: [:],
        methods: [
            "addListener": [
                .plain(.function([.plain(.jsLockHandleRemote)] => .undefined))
            ] => .integer
        ]
    )

    fileprivate static let lockRequestFailedCallbackReceiver = ObjectGroup(
        name: MojoStrings.lockRequestFailedCallbackReceiver,
        instanceType: .jsLockRequestFailedCallbackReceiver,
        properties: [:],
        methods: [
            "addListener": [.plain(.function([] => .undefined))] => .integer
        ]
    )

    fileprivate static let lockRequestRemote = ObjectGroup(
        name: MojoStrings.lockRequestRemote,
        instanceType: .jsLockRequestRemote,
        properties: [:],
        methods: [:]
    )

    fileprivate static let lockHandleCallbackRouter = ObjectGroup(
        name: MojoStrings.lockHandleCallbackRouter,
        instanceType: .jsLockHandleCallbackRouter,
        properties: [
            "$": .jsLockHandleCallbackRouterReceiverHelper
        ],
        methods: [:]
    )

    fileprivate static let lockHandleCallbackRouterReceiverHelper = ObjectGroup(
        name: MojoStrings.lockHandleCallbackRouterReceiverHelper,
        instanceType: .jsLockHandleCallbackRouterReceiverHelper,
        properties: [:],
        methods: [
            "associateAndPassRemote": [] => .jsLockHandleRemote,
            "closeBindings": [] => .undefined,
        ]
    )

    fileprivate static let lockHandleRemote = ObjectGroup(
        name: MojoStrings.lockHandleRemote,
        instanceType: .jsLockHandleRemote,
        properties: [
            "$": .jsLockHandleRemoteWrapper
        ],
        methods: [:]
    )

    fileprivate static let lockHandleRemoteWrapper = ObjectGroup(
        name: MojoStrings.lockHandleRemoteWrapper,
        instanceType: .jsLockHandleRemoteWrapper,
        properties: [:],
        methods: [
            "close": [] => .undefined
        ]
    )

    fileprivate static let lockInfo = ObjectGroup(
        name: MojoStrings.lockInfo,
        instanceType: .jsLockInfo,
        properties: [
            "name": .string,
            "mode": .jsLockMode,
            "client_id": .string,
        ],
        methods: [:]
    )
}

private let mojoBuiltins: [String: ILType] = [
    MojoStrings.getOrCreatePrimaryRemote: .function([] => .jsLockManagerRemote),

    MojoStrings.lockManager: .jsLockManager,
    MojoStrings.lockRequestCallbackRouter: .constructor(
        [] => .jsLockRequestCallbackRouter),
    MojoStrings.lockHandleCallbackRouter: .constructor(
        [] => .jsLockHandleCallbackRouter),
]

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
    // Pick a variable among all Mojo variables with methods
    var targetVar = b.findVariable {
        let type = b.type(of: $0)
        return isTargetObject(type: type) && !type.methods.isEmpty
            && type.group != MojoStrings.lockManager
    }

    // If we can't find a Mojo object in scope, create the LockManager remote
    guard let obj = targetVar else {
        let getRemoteFunc = b.createNamedVariable(forBuiltin: MojoStrings.getOrCreatePrimaryRemote)
        b.callFunction(getRemoteFunc, withArgs: [])
        return
    }

    if probability(0.008) {
        b.loadString("EXPERIMENTAL_lock_manager_crash")
    }

    let methodName = b.type(of: obj).randomMethod()!
    let signatures = b.methodSignatures(of: methodName, on: obj)
    let signature = chooseUniform(from: signatures)
    let arguments = b.findOrGenerateArguments(
        forSignature: signature, maxNumberOfVariablesToGenerate: 500)
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

let mojoLockManagerProfile = Profile(
    processArgs: { _ in return [] },
    processArgsReference: nil,
    processEnv: [
        "ASAN_OPTIONS": "detect_odr_violation=0:abort_on_error=1", "DISPLAY": ":20",
    ],
    maxExecsBeforeRespawn: 1000,
    timeout: Timeout.interval(11000, 11000),
    codePrefix: """
        let globalRemote = null;
        function getOrCreatePrimaryRemote() {
            if (!globalRemote) {
                globalRemote = blink.mojom.LockManager.getRemote();
            }
            return globalRemote;
        }
        """,
    codeSuffix: "if (globalRemote) {globalRemote.$.close()}",
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
        (MojoMethodCallGenerator, 10000),
        (MojoPropertyRetrievalGenerator, 10000),
        (MojoRouterListenerGenerator, 5000),
    ] + commonMojoCodeGenerators,
    additionalProgramTemplates: WeightedList([]),
    disabledCodeGenerators: mojoDisabledGenerators,
    disabledMutators: [
        "ExplorationMutator", "ProbingMutator", "PropertyAccessorMutator"
    ],
    additionalBuiltins: mojoBuiltins.merging(commonMojoBuiltins) { (existing, _) in existing },
    additionalObjectGroups: [
        .lockManager,
        .lockManagerRemote,
        .lockManagerRemoteWrapper,
        .blinkMojomLockManagerQueryStateResponse,
        .lockRequestCallbackRouter,
        .lockRequestCallbackRouterReceiverHelper,
        .lockRequestGrantedCallbackReceiver,
        .lockRequestFailedCallbackReceiver,
        .lockRequestRemote,
        .lockHandleCallbackRouter,
        .lockHandleCallbackRouterReceiverHelper,
        .lockHandleRemote,
        .lockHandleRemoteWrapper,
        .lockInfo,
    ] + commonMojoObjectGroups,
    additionalEnumerations: [
        .jsLockMode, .jsWaitMode,
    ] + commonMojoEnumerations,
    additionalOptionsBags: [] + commonMojoOptionsBags,
    optionalPostProcessor: nil
)
