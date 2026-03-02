// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Fuzzilli

extension ILType {
  // LockManager
  fileprivate static let jsLockManager: ILType = .object(
    ofGroup: "blink.mojom.LockManager", withProperties: ["$interfaceName"])
  fileprivate static let jsLockManagerRemote: ILType = .object(
    ofGroup: "blink.mojom.LockManagerRemote", withProperties: ["$"])
  fileprivate static let jsLockManagerRemoteWrapper: ILType = .object(
    ofGroup: "LockManagerRemoteWrapper")

  // LockRequest
  fileprivate static let jsLockRequestCallbackRouter: ILType = .object(
    ofGroup: "blink.mojom.LockRequestCallbackRouter", withProperties: ["$", "granted", "failed"])
  fileprivate static let jsLockRequestCallbackRouterReceiverHelper: ILType = .object(
    ofGroup: "LockRequestCallbackRouterReceiverHelper")
  fileprivate static let jsLockRequestRemote: ILType = .object(
    ofGroup: "blink.mojom.LockRequestRemote")

  // Callback Receivers for LockRequest
  fileprivate static let jsLockRequestGrantedCallbackReceiver: ILType = .object(
    ofGroup: "LockRequestGrantedCallbackReceiver")
  fileprivate static let jsLockRequestFailedCallbackReceiver: ILType = .object(
    ofGroup: "LockRequestFailedCallbackReceiver")

  // LockHandle
  fileprivate static let jsLockHandleCallbackRouter: ILType = .object(
    ofGroup: "blink.mojom.LockHandleCallbackRouter", withProperties: ["$"])
  fileprivate static let jsLockHandleCallbackRouterReceiverHelper: ILType = .object(
    ofGroup: "LockHandleCallbackRouterReceiverHelper")
  fileprivate static let jsLockHandleRemote: ILType = .object(
    ofGroup: "blink.mojom.LockHandleRemote")

  // Data types
  fileprivate static let jsLockMode: ILType = .integer
  fileprivate static let jsWaitMode: ILType = .integer
  fileprivate static let jsLockInfo: ILType = .object(
    ofGroup: "LockInfo", withProperties: ["name", "mode", "client_id"])
}

private let lockManager = ObjectGroup(
  name: "blink.mojom.LockManager",
  instanceType: .jsLockManager,
  properties: [
    "$interfaceName": .string
  ],
  methods: [
    "getRemote": [] => .jsLockManagerRemote
  ]
)

private let lockManagerRemote = ObjectGroup(
  name: "blink.mojom.LockManagerRemote",
  instanceType: .jsLockManagerRemote,
  properties: [
    "$": .jsLockManagerRemoteWrapper
  ],
  methods: [
    "requestLock": [
      .string, .plain(.jsLockMode), .plain(.jsWaitMode), .plain(.jsLockRequestRemote),
    ] => .undefined,
    "queryState": [] => .jsPromise,  // Returns {requested: [LockInfo], held: [LockInfo]}
  ]
)

private let lockManagerRemoteWrapper = ObjectGroup(
  name: "LockManagerRemoteWrapper",
  instanceType: .jsLockManagerRemoteWrapper,
  properties: [:],
  methods: [
    "close": [] => .undefined,
    "isBound": [] => .boolean,
  ]
)

private let lockRequestCallbackRouter = ObjectGroup(
  name: "blink.mojom.LockRequestCallbackRouter",
  instanceType: .jsLockRequestCallbackRouter,
  properties: [
    "$": .jsLockRequestCallbackRouterReceiverHelper,
    "granted": .jsLockRequestGrantedCallbackReceiver,
    "failed": .jsLockRequestFailedCallbackReceiver
  ],
  methods: [
    "removeListener": [.integer] => .boolean
  ]
)

private let lockRequestCallbackRouterReceiverHelper = ObjectGroup(
  name: "LockRequestCallbackRouterReceiverHelper",
  instanceType: .jsLockRequestCallbackRouterReceiverHelper,
  properties: [:],
  methods: [
    "associateAndPassRemote": [] => .jsLockRequestRemote
  ]
)

private let lockRequestGrantedCallbackReceiver = ObjectGroup(
    name: "LockRequestGrantedCallbackReceiver",
    instanceType: .jsLockRequestGrantedCallbackReceiver,
    properties: [:],
    methods: [
    "addListener": [.plain(.function([.plain(.jsLockHandleRemote)] => .undefined))] => .integer
    ]
)

private let lockRequestFailedCallbackReceiver = ObjectGroup(
    name: "LockRequestFailedCallbackReceiver",
    instanceType: .jsLockRequestFailedCallbackReceiver,
    properties: [:],
    methods: [
    "addListener": [.plain(.function([] => .undefined))] => .integer
    ]
)

private let lockRequestRemote = ObjectGroup(
  name: "blink.mojom.LockRequestRemote",
  instanceType: .jsLockRequestRemote,
  properties: [:],
  methods: [:]
)

private let lockHandleCallbackRouter = ObjectGroup(
  name: "blink.mojom.LockHandleCallbackRouter",
  instanceType: .jsLockHandleCallbackRouter,
  properties: [
    "$": .jsLockHandleCallbackRouterReceiverHelper
  ],
  methods: [
    "removeListener": [.integer] => .boolean
  ]
)

private let lockHandleCallbackRouterReceiverHelper = ObjectGroup(
  name: "LockHandleCallbackRouterReceiverHelper",
  instanceType: .jsLockHandleCallbackRouterReceiverHelper,
  properties: [:],
  methods: [
    "associateAndPassRemote": [] => .jsLockHandleRemote
  ]
)

private let lockHandleRemote = ObjectGroup(
  name: "blink.mojom.LockHandleRemote",
  instanceType: .jsLockHandleRemote,
  properties: [:],
  methods: [:]
)

private let lockInfo = ObjectGroup(
  name: "LockInfo",
  instanceType: .jsLockInfo,
  properties: [
    "name": .string,
    "mode": .jsLockMode,
    "client_id": .string,
  ],
  methods: [:]
)

private let mojoBuiltins: [String: ILType] = [
  "blink.mojom.LockManager": .jsLockManager,
  "blink.mojom.LockManagerRemote": .constructor([] => .jsLockManagerRemote),
  "blink.mojom.LockRequestCallbackRouter": .constructor([] => .jsLockRequestCallbackRouter),
  "blink.mojom.LockHandleCallbackRouter": .constructor([] => .jsLockHandleCallbackRouter),
  "blink.mojom.LockHandleRemote": .constructor([] => .jsLockHandleRemote),
  "blink.mojom.LockMode": .object(withProperties: ["SHARED", "EXCLUSIVE"]),
  "blink.mojom.LockManager.WaitMode": .object(withProperties: ["WAIT", "NO_WAIT", "PREEMPT"]),
]

// Program Template to force Mojo usage
private let MojoLockManagerFuzzer = ProgramTemplate("MojoLockManagerFuzzer") { b in
  b.buildPrefix()

  // Get the LockManager remote
  let managerStatic = b.createNamedVariable(forBuiltin: "blink.mojom.LockManager")
  let manager = b.callMethod("getRemote", on: managerStatic, withArgs: [])

  // Generate random code to use the objects further
  b.build(n: 20)
}

let chromiumMojoProfile = Profile(
  processArgs: { _ in return [] },
  processArgsReference: nil,
  processEnv: ["ASAN_OPTIONS": "detect_odr_violation=0:abort_on_error=1", "DISPLAY": ":20"],
  maxExecsBeforeRespawn: 1000,
  timeout: Timeout.interval(11000, 11000),
  codePrefix: v8Profile.codePrefix,
  codeSuffix: v8Profile.codeSuffix,
  ecmaVersion: v8Profile.ecmaVersion,
  startupTests: [
    ("fuzzilli('FUZZILLI_PRINT', 'test')", .shouldSucceed),
    (
      "if (typeof blink === 'undefined' || typeof blink.mojom === 'undefined' || typeof blink.mojom.LockManager === 'undefined') throw 'LockManager not found'",
      .shouldSucceed
    ),
  ] + v8Profile.startupTests,
  additionalCodeGenerators: [],
  additionalProgramTemplates: WeightedList([
    // Heavily bias Fuzzilli to use the ProgramTemplate that establishes a Mojo connection.
    (MojoLockManagerFuzzer, 1000),
  ]),
  disabledCodeGenerators: v8Profile.disabledCodeGenerators,
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
    lockInfo,
  ],
  additionalEnumerations: v8Profile.additionalEnumerations,
  optionalPostProcessor: nil
)
