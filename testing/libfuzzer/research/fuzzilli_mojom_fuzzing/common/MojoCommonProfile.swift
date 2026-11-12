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
///    - Module and type name, separated by dots (e.g.
///      "mojoBase.mojom.String16", "url.mojom.Url").
/// 2. `ILType` names:
///    - The unique name is the namespace concatenated with the type name, in
///      PascalCase (e.g. "MojoBaseMojomString16", "UrlMojomUrl",
///      "SkiaMojomBitmapN32").
///    - ILType: `js<UniqueName>` (e.g. `jsMojoBaseMojomString16`).
///    - Constructors: `js<UniqueName>Constructor`.
/// 3. `ObjectGroup` and `OptionsBag` names:
///    - Namespace concatenated with the type name, in camelCase (e.g.
///      "urlMojomUrl")
/// 4. CodeGenerator names:
///    - `Mojo<namespace><type name>Generator` (e.g. `MojoUrlMojomUrlGenerator`)
/// 5. Module lists (of ObjectGroups, CodeGenerators, etc.):
///    - Namespace concatenated with the registry kind, in camelCase (e.g.
///      `urlMojomObjectGroups`, `mojoBaseMojomCodeGenerators`).

public enum CommonMojoStrings {}

/// Number of executions before Fuzzilli restarts the target process.
public let mojoMaxExecsBeforeRespawn = 250

/// Upper bound that `mojoCommonCodePrefix` puts on `Mojo.createSharedBuffer`.
public let mojoMaxSharedBufferSize = 4 * 1024 * 1024

/// Maximum number of Mojo `sendMessage` calls allowed per program execution
/// before further calls become no-ops, preventing V8 loop/mutation amplification
/// from stalling IPC drainage and triggering full GC pauses.
public let mojoMaxIpcCallsPerProgram = 150

/// How long `nextCall` in `mojoCommonCodePrefix` waits for the browser to call
/// an interface that JS implements. Programs pay this whenever the call does
/// not come, so it is short.
public let mojoCallbackTimeoutMs = 150

/// How long `mojoCommonCodePrefix` lets a call wait for its reply.
public let mojoReplyTimeoutMs = 1000

/// How long `mojoCommonCodePrefix` lets all the calls of a program together
/// wait for replies that are late. Profiles also use it in `codeSuffix` as the
/// time after which a program no longer waits for its async functions. It is
/// well below the timeout of the profiles, after which Fuzzilli kills and
/// restarts the process.
public let mojoProgramBudgetMs = 3000

/// Probability that a program ends by firing a few un-awaited calls on live
/// remotes and immediately closing the primary remote without awaiting their
/// replies, disconnecting the primary pipe and all associated endpoints in the
/// browser while those messages are in flight.
public let mojoFinalPrimaryTeardownProbability = 0.25

/// Generator weights shared across all Mojo profiles.
public let mojoAwaitedCallWeight = 13000
public let mojoMethodCallWeight = 6000
public let mojoTeardownWeight = 1800
public let mojoNextCallWeight = 500
public let mojoPropertyRetrievalWeight = 500
public let mojoRouterListenerWeight = 2500
public let mojoReplyChainWeight = 3500
public let mojoAsyncFunctionWeight = 1000
public let mojoStringGeneratorWeight = 800
public let mojoIntegerGeneratorWeight = 400
public let mojoFloatGeneratorWeight = 300
public let mojoBigIntGeneratorWeight = 200
public let mojoStructMutateWeight = 450
public let mojoDataPipeGeneratorWeight = 150

public let commonMojoBuiltins: [String: ILType] = [:]
    .merging(mojoCoreBuiltins) { (existing, _) in existing }
    .merging(mojoBaseMojomBuiltins) { (existing, _) in existing }
    .merging(skiaMojomBuiltins) { (existing, _) in existing }
    .merging(urlMojomBuiltins) { (existing, _) in existing }
    .merging(webnnMojomBuiltins) { (existing, _) in existing }

public let commonMojoCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoAwaitGenerator, 1000),
    (MojoStringGenerator, mojoStringGeneratorWeight),
    (MojoIntegerGenerator, mojoIntegerGeneratorWeight),
    (MojoFloatGenerator, mojoFloatGeneratorWeight),
    (MojoBigIntGenerator, mojoBigIntGeneratorWeight),
    (MojoObjectLiteralNoopGenerator, 1),
    (MojoBufferSourceGenerator, 1),
]
    + mojoCoreCodeGenerators

private func expandIgnoredTypes(_ ignoredTypes: Set<String>) -> Set<String> {
    var expanded = ignoredTypes
    if expanded.contains("mojoBase.mojom.BigString16")
        || expanded.contains("mojoBase.mojom.BigString")
    {
        expanded.insert("mojoBase.mojom.BigBuffer")
    }
    if expanded.contains("mojoBase.mojom.BigBuffer") {
        expanded.insert("mojoBase.mojom.BigBufferSharedMemoryRegion")
    }
    return expanded
}

/// Returns the shared builtins needed by a profile that references `ignoredTypes`,
/// omitting external mojom constructors whose scripts page is not loaded.
public func commonMojoBuiltins(for ignoredTypes: Set<String>) -> [String: ILType] {
    let types = expandIgnoredTypes(ignoredTypes)
    var result = mojoCoreBuiltins
    let candidates: [(String, [String: ILType])] = [
        ("mojoBase.mojom.String16", mojoBaseMojomBuiltins.filter { $0.key == CommonMojoStrings.mojoBaseMojomString16 }),
        ("mojoBase.mojom.BigBuffer", mojoBaseMojomBuiltins.filter { $0.key.hasPrefix("mojoBase.mojom.BigBuffer.") }),
        ("mojoBase.mojom.BigBufferSharedMemoryRegion", mojoBaseMojomBuiltins.filter { $0.key == CommonMojoStrings.mojoBaseMojomBigBufferSharedMemoryRegion }),
        ("mojoBase.mojom.BigString16", mojoBaseMojomBuiltins.filter { $0.key == CommonMojoStrings.mojoBaseMojomBigString16 }),
        ("mojoBase.mojom.Uint128", mojoBaseMojomBuiltins.filter { $0.key == CommonMojoStrings.mojoBaseMojomUint128 }),
        ("skia.mojom.BitmapN32ImageInfo", skiaMojomBuiltins),
        ("url.mojom.Url", urlMojomBuiltins.filter { $0.key == CommonMojoStrings.urlMojomUrl }),
        ("url.mojom.SchemeHostPort", urlMojomBuiltins.filter { $0.key == CommonMojoStrings.urlMojomSchemeHostPort }),
        ("webnn.mojom.OperandDescriptor", webnnMojomBuiltins),
    ]
    for (typeName, builtins) in candidates where types.contains(typeName) {
        result.merge(builtins) { (existing, _) in existing }
    }
    return result
}

/// Returns the shared code generators for a profile that references `ignoredTypes`.
public func commonMojoCodeGenerators(for ignoredTypes: Set<String>) -> [(CodeGenerator, Int)] {
    let types = expandIgnoredTypes(ignoredTypes)
    var result = commonMojoCodeGenerators
    if types.contains("mojoBase.mojom.BigBuffer") {
        result.append((MojoMojoBaseMojomBigBufferUnionBytesGenerator, 1))
    }
    if types.contains("mojoBase.mojom.BigBufferSharedMemoryRegion") {
        result.append((MojoMojoBaseMojomBigBufferSharedMemoryRegionGenerator, 1))
    }
    if types.contains("mojoBase.mojom.String16") {
        result.append((MojoMojoBaseMojomString16Generator, 1))
    }
    if types.contains("skia.mojom.BitmapN32ImageInfo") {
        result += skiaMojomCodeGenerators
    }
    if types.contains("url.mojom.Url") {
        result.append((MojoUrlMojomUrlGenerator, 1))
    }
    if types.contains("url.mojom.SchemeHostPort") {
        result.append((MojoUrlMojomUrlSchemeHostPortGenerator, 1))
    }
    if types.contains("webnn.mojom.OperandDescriptor") {
        result += webnnMojomCodeGenerators
    }
    return result
}

public let commonMojoObjectGroups: [ObjectGroup] = []
    + mojoCoreObjectGroups
    + mojoBaseMojomObjectGroups
    + skiaMojomObjectGroups
    + urlMojomObjectGroups
    + webnnMojomObjectGroups

public let commonMojoEnumerations: [ILType] = []
    + networkMojomEnumerations
    + skiaMojomEnumerations
    + webnnMojomEnumerations

public let commonMojoOptionsBags: [OptionsBag] = []
    + mojoCoreOptionsBags

extension ILType {
    // TODO(crbug.com/554102710): Upstream `jsBufferSource` to be a builtin type in Fuzzilli
    public static let jsBufferSource: ILType =
        JavaScriptEnvironment.typedArrayConstructors
        .map { jsTypedArray($0) }
        .reduce(.jsArrayBuffer | .jsDataView, |)
}

/// `Profile.processArgs` takes one argument up to Fuzzilli ead28307 and two
/// from 03c63dec on. Profiles pass this function instead of a closure, and
/// overload resolution picks the variant that the Fuzzilli checkout expects.
public func mojoProcessArgs(_ randomize: Bool) -> [String] {
    return []
}

public func mojoProcessArgs(_ randomize: Bool, _ isWasmEnabled: Bool) -> [String] {
    return []
}

/// JavaScript that every profile puts in its `codePrefix`.
///
/// It wraps six functions of the bindings and adds one. Each program installs
/// its own wrappers around the
/// originals, which it finds saved next to them, rather than keeping those of
/// an earlier program: consecutive programs can share a JavaScript realm, and
/// when they do not, a wrapper created by an earlier program belongs to a realm
/// that may be gone.
public let mojoCommonCodePrefix = """
    {
        // Mutators turn the size of a shared buffer into any integer, and a
        // program that maps and fills gigabytes gets the process killed. The
        // sizes that interfaces validate are the ones in messages, which stay
        // unbounded.
        const createSharedBuffer =
            (Mojo.mojoFuzzCreateSharedBuffer ??= Mojo.createSharedBuffer);
        Mojo.createSharedBuffer = function(numBytes) {
            return createSharedBuffer.call(
                Mojo, Math.min(numBytes >>> 0, \(mojoMaxSharedBufferSize)));
        };
        // A call on a remote whose pipe is closed is a no-op whose reply is
        // `undefined`, instead of a rejected promise or a TypeError. A program
        // can then close a remote, or have the browser close it, and keep
        // going: awaiting a call that was in flight does not end the function.
        // Arguments that cannot be serialized still throw.
        //
        // A reply that does not come within \(mojoReplyTimeoutMs) ms is `undefined` as well.
        // Some replies never come, e.g. when the browser waits for the user,
        // and a program awaiting one would run until Fuzzilli's timeout kills
        // the process. All the calls of a program together wait no longer than
        // \(mojoProgramBudgetMs) ms for late replies.
        const remoteBase = mojo.internal.interfaceSupport.InterfaceRemoteBase.prototype;
        const sendMessage = (remoteBase.mojoFuzzSendMessage ??= remoteBase.sendMessage);
        const deadline = performance.now() + \(mojoProgramBudgetMs);
        let remainingCalls = \(mojoMaxIpcCallsPerProgram);
        remoteBase.sendMessage = function(...args) {
            if (!this.endpoint_ || !this.endpoint_.isStarted || !this.endpoint_.router_
                || --remainingCalls < 0) {
                return Promise.resolve(undefined);
            }
            let reply;
            try {
                reply = sendMessage.apply(this, args).catch(() => undefined);
            } catch {
                return Promise.resolve(undefined);
            }
            const expectsReply = !!args[2];
            if (!expectsReply) {
                return reply;
            }
            return new Promise((resolve) => {
                const wait = Math.min(\(mojoReplyTimeoutMs), deadline - performance.now());
                const timer = setTimeout(resolve, Math.max(0, wait));
                reply.then((value) => {
                    clearTimeout(timer);
                    resolve(value);
                });
            });
        };
        // Binding a reply `PendingReceiver` returned as `null` when the call
        // failed (or mutated to a non-handle) becomes a clean no-op.
        const receiverHelper = mojo.internal.interfaceSupport.InterfaceReceiverHelper.prototype;
        const bindHandle = (receiverHelper.mojoFuzzBindHandle ??= receiverHelper.bindHandle);
        receiverHelper.bindHandle = function(handle) {
            if (!handle || (typeof handle.watch !== 'function' && typeof handle.start !== 'function')) {
                return;
            }
            return bindHandle.call(this, handle);
        };
        // A remote that was never bound is sent as a remote whose receiver is
        // already gone, instead of throwing a TypeError: the browser then gets
        // a message, and has to cope with a remote that is disconnected.
        const unbind = (remoteBase.mojoFuzzUnbind ??= remoteBase.unbind);
        remoteBase.unbind = function() {
            if (!this.endpoint_) {
                this.bindNewPipeAndPassReceiver().handle.close();
            }
            return unbind.call(this);
        };
        // Interfaces that JS implements.
        //
        // `router.method.nextCall(...names)` returns a promise of the arguments
        // of the next call of `method` by the browser, as an object with one
        // property per name. It is `undefined` if the browser calls another
        // method of the interface first, or none within \(mojoCallbackTimeoutMs) ms.
        const callbackReceiver =
            mojo.internal.interfaceSupport.InterfaceCallbackReceiver.prototype;
        callbackReceiver.nextCall = function(...names) {
            const routerWaiters = (this.callbackRouter_.mojoFuzzWaiters ??= new Set());
            const methodWaiters = (this.mojoFuzzMethodWaiters ??= new Set());
            return new Promise((resolve) => {
                const entry = {
                    names,
                    settle(value) {
                        clearTimeout(timer);
                        routerWaiters.delete(entry);
                        methodWaiters.delete(entry);
                        resolve(value);
                    },
                };
                const timer = setTimeout(() => entry.settle(undefined), \(mojoCallbackTimeoutMs));
                routerWaiters.add(entry);
                methodWaiters.add(entry);
            });
        };
        // Every call of the browser settles `nextCall` waiters first (before
        // any user listener can close the router or throw), and ends the wait
        // for the other methods of the interface. For methods with a reply,
        // `dispatchWithResponse_` takes the last defined reply from the
        // listeners or rejects cleanly when no listener provides one.
        for (const name of ['dispatch_', 'dispatchWithResponse_']) {
            const dispatch = (callbackReceiver['mojoFuzz_' + name] ??= callbackReceiver[name]);
            callbackReceiver[name] = function(...args) {
                for (const entry of [...(this.mojoFuzzMethodWaiters ?? [])]) {
                    entry.settle(
                        Object.fromEntries(entry.names.map((n, i) => [n, args[i]])));
                }
                for (const entry of [...(this.callbackRouter_.mojoFuzzWaiters ?? [])]) {
                    entry.settle(undefined);
                }
                if (name === 'dispatchWithResponse_') {
                    const values = [];
                    for (const listener of this.listeners_.values()) {
                        try {
                            values.push(listener.apply(null, args));
                        } catch {}
                    }
                    return Promise.all(values).then((resolved) => {
                        for (let i = resolved.length - 1; i >= 0; i--) {
                            if (resolved[i] !== undefined) {
                                return resolved[i];
                            }
                        }
                        return Promise.reject();
                    });
                }
                return dispatch.apply(this, args);
            };
        }
        // A listener that is an async function keeps the program waiting
        // while it runs, like the async functions that the program starts.
        const addListener =
            (callbackReceiver.mojoFuzzAddListener ??= callbackReceiver.addListener);
        callbackReceiver.addListener = function(listener) {
            return addListener.call(this, (...args) => {
                const result = listener(...args);
                if (result instanceof Promise) {
                    mojoAsyncFunctionPromises.push(result);
                }
                return result;
            });
        };
    }
    """

/// Opens an async function, in which Fuzzilli can await Mojo calls and use the
/// replies, and runs it.
///
/// This generator is what provides the `.async` context. Whenever Fuzzilli
/// picks a generator that needs that context (the awaited calls, the reply
/// chains, the awaits) outside an async function, it opens one with this
/// generator and runs the picked generator inside.
///
/// The function variable is hidden for good, so that no generated code calls it
/// again, in particular not its own body. The promise it returns is pushed to
/// `mojoAsyncFunctionPromises`, which every profile declares in `codePrefix`
/// and awaits in `codeSuffix`, so that the function runs to completion.
public func makeMojoAsyncFunctionGenerator(
    finalTeardown: @escaping (ProgramBuilder) -> Void = { _ in }
) -> CodeGenerator {
    return CodeGenerator(
        "MojoAsyncFunctionGenerator",
        [
            GeneratorStub(
                "MojoAsyncFunctionBeginGenerator",
                provides: [.javascript, .async]
            ) { b in
                let descriptor: ProgramBuilder.SubroutineDescriptor = .parameters(n: 0)
                b.setParameterTypesForNextSubroutine(descriptor.parameterTypes)
                let function = b.emit(
                    BeginAsyncArrowFunction(parameters: descriptor.parameters)
                ).output
                b.hide(function)
                b.runtimeData.push(mojoAsyncFunctionKey, function)
                // Every sequence of calls starts at the primary remote.
                let getRemote = b.createNamedVariable(forBuiltin: "getOrCreatePrimaryRemote")
                b.callFunction(getRemote, withArgs: [])
                b.hide(getRemote)
            },
            GeneratorStub(
                "MojoAsyncFunctionEndGenerator",
                inContext: .single([.javascript, .async])
            ) { b in
                if probability(mojoFinalPrimaryTeardownProbability) {
                    finalTeardown(b)
                }
                b.emit(EndAsyncArrowFunction())
                let function = b.runtimeData.pop(mojoAsyncFunctionKey)
                mojoRunAsyncFunction(b, function)
            },
        ])
}

public let MojoAsyncFunctionGenerator = makeMojoAsyncFunctionGenerator()

private let mojoAsyncFunctionKey = "mojoAsyncFunction"

/// Calls the async function `function` and pushes its promise to
/// `mojoAsyncFunctionPromises`.
private func mojoRunAsyncFunction(_ b: ProgramBuilder, _ function: Variable) {
    let promise = b.callFunction(function, withArgs: [])
    let promises = b.createNamedVariable(forBuiltin: "mojoAsyncFunctionPromises")
    b.callMethod("push", on: promises, withArgs: [promise])
    b.hide(promise)
    b.hide(promises)
}

/// Mojo copy of the builtin AwaitGenerator so that its weight can be tuned for
/// this profile without affecting the global codeGeneratorWeights. The builtin
/// "AwaitGenerator" is disabled for this profile.
public let MojoAwaitGenerator = CodeGenerator(
    "MojoAwaitGenerator", inContext: .single(.async),
    inputs: .preferred(.thenable)
) { b, val in
    b.await(val)
}

/// Rewrites every program right before it runs, whichever engine or mutator
/// made it. It does four things:
///
/// 1. Rejects programs in which a function calls itself. Splicing creates
///    them, by copying the call of an async function into that function. They
///    run until the stack overflows or the program times out, and long-running
///    programs took over the corpora of earlier campaigns.
/// 2. Repairs async functions. Mutators and the minimizer remove the call of
///    a function or the push of its promise, which leaves dead code or a
///    function that is still running when the program ends. A function that
///    nothing uses is called where it is defined, and its promise is pushed.
/// 3. Extends the program with Mojo calls. Corpus programs are minimized to
///    what triggered their coverage, so a mutated program often makes few
///    calls, or none. If the program makes fewer than `minMojoCalls`, and
///    otherwise with probability `extensionProbability`, code is generated at
///    the end of the body of its last async function, where the remotes that
///    the function already holds are in scope. Each execution then continues
///    the session of its parent instead of only replaying it.
/// 4. Fixes references to Mojo builtins that go through `globalThis`. Fuzzilli
///    models every builtin as a property of `globalThis` too, and so sometimes
///    emits `globalThis["blink.mojom.Foo"]` for the builtin `blink.mojom.Foo`.
///    That is `undefined`, since the name is a path and not a property, and
///    constructing or calling it throws. These become `blink.mojom.Foo`.
public struct MojoFuzzingPostProcessor: FuzzingPostProcessor {
    private let minMojoCalls = 3
    private let extensionProbability = 0.3
    private let finalTeardown: ((ProgramBuilder) -> Void)?

    public init(finalTeardown: ((ProgramBuilder) -> Void)? = nil) {
        self.finalTeardown = finalTeardown
    }

    public func process(_ program: Program, for fuzzer: Fuzzer) throws -> Program {
        let primaryGroup =
            fuzzer.environment.type(ofBuiltin: "getOrCreatePrimaryRemote")
            .signature?.outputType.group ?? ""
        let primaryWrapperGroup = primaryGroup.isEmpty ? "" : primaryGroup + "Wrapper"

        // Maps the index of the instruction ending an async function to the
        // function, for every async function that nothing uses.
        var unusedAsyncFunctions = [Int: Variable]()
        // The index of the instruction ending the last async function that is
        // defined outside any block.
        var extensionPoint: Int? = nil
        // The promises of async functions called outside any block.
        var promises = [Variable]()
        var pushedValues = Set<Variable>()

        var asyncFunctions = [Variable: Int]()
        var usedVariables = Set<Variable>()
        var openFunctions = [(function: Variable, isTopLevelAsync: Bool)]()
        var depth = 0
        for instr in program.code {
            usedVariables.formUnion(instr.inputs)
            if instr.isBlockEnd { depth -= 1 }
            if instr.op is BeginAnyFunction {
                let isAsync = instr.op is BeginAsyncArrowFunction
                openFunctions.append((instr.output, isAsync && depth == 0))
                if isAsync { asyncFunctions[instr.output] = -1 }
            } else if instr.op is EndAnyFunction {
                let (function, isTopLevelAsync) = openFunctions.removeLast()
                if asyncFunctions[function] != nil { asyncFunctions[function] = instr.index }
                if isTopLevelAsync { extensionPoint = instr.index }
            } else if instr.op is CallFunction || instr.op is CallFunctionWithSpread
                || instr.op is Construct || instr.op is ConstructWithSpread
            {
                let callee = instr.input(0)
                if openFunctions.contains(where: { $0.function == callee }) {
                    throw InternalError.postProcessRejection("recursive call")
                }
                if depth == 0, asyncFunctions[callee] != nil {
                    promises.append(instr.output)
                }
            } else if let op = instr.op as? CallMethod, op.methodName == "push" {
                pushedValues.formUnion(instr.inputs.dropFirst())
            }
            if instr.isBlockStart { depth += 1 }
        }
        for (function, end) in asyncFunctions where !usedVariables.contains(function) {
            unusedAsyncFunctions[end] = function
        }

        var numMojoCalls = 0
        func shouldExtend() -> Bool {
            return numMojoCalls < minMojoCalls || probability(extensionProbability)
        }

        // The endpoints that calls of the program have sent. Sending one moves
        // its pipe to the browser, so the generated code must not send or use
        // it again. The generators hide such variables as they go, but that
        // does not outlive the builder that made the program.
        var sentEndpoints = Set<Variable>()
        func hideUnusableVariables(_ b: ProgramBuilder) {
            for variable in b.visibleVariables {
                let group = b.type(of: variable).group ?? ""
                // The `$` of a remote can only bind it again, which drops its pipe.
                if sentEndpoints.contains(variable) || group.hasSuffix("RemoteWrapper") {
                    b.hide(variable)
                }
            }
        }

        let b = fuzzer.makeBuilder(forMutating: program)
        b.adopting {
            for instr in program.code {
                if instr.index == extensionPoint, !(program.code[instr.index - 1].op is Return) {
                    if shouldExtend() {
                        hideUnusableVariables(b)
                        // Code generation needs a few variables to work with.
                        if b.numberOfVisibleJsVariables < 3 {
                            b.buildValues(3 - b.numberOfVisibleJsVariables)
                        }
                        b.build(n: Int.random(in: 5...15), by: .generating)
                    }
                    if probability(mojoFinalPrimaryTeardownProbability) {
                        hideUnusableVariables(b)
                        finalTeardown?(b)
                    }
                }
                if let op = instr.op as? CallMethod, op.methodName == "close",
                    !primaryWrapperGroup.isEmpty,
                    b.type(of: b.adopt(instr.input(0))).group == primaryWrapperGroup
                {
                    let output = b.adopt(instr.output)
                    b.append(Instruction(LoadUndefined(), output: output))
                } else {
                    b.adopt(instr)
                }
                if instr.op is CallMethod {
                    if let group = b.type(of: b.adopt(instr.input(0))).group,
                        group.hasSuffix("Remote")
                    {
                        numMojoCalls += 1
                    }
                    for argument in b.adopt(instr.inputs.dropFirst()) {
                        let group = b.type(of: argument).group ?? ""
                        if group.hasSuffix("Remote") || group.hasSuffix("PendingReceiver")
                            || group.hasSuffix("PendingAssociatedReceiver")
                        {
                            sentEndpoints.insert(argument)
                        }
                    }
                }
                if let function = unusedAsyncFunctions[instr.index] {
                    mojoRunAsyncFunction(b, b.adopt(function))
                }
            }

            for promise in promises where !pushedValues.contains(promise) {
                let array = b.createNamedVariable(forBuiltin: "mojoAsyncFunctionPromises")
                b.callMethod("push", on: array, withArgs: [b.adopt(promise)])
            }
        }

        // A program without an async function gets a new one.
        if extensionPoint == nil, shouldExtend(),
            let generator = fuzzer.codeGenerators.first(where: {
                $0.name == MojoAsyncFunctionGenerator.name
            })
        {
            hideUnusableVariables(b)
            let _ = b.complete(generator: generator, withBudget: Int.random(in: 10...20))
        }

        return fixBuiltinPaths(in: b.finalize(), for: fuzzer)
    }

    /// Returns `program` with its references to builtins through `globalThis`
    /// fixed. This is a pass of its own so that it also covers the code that
    /// was just generated.
    private func fixBuiltinPaths(in program: Program, for fuzzer: Fuzzer) -> Program {
        let b = fuzzer.makeBuilder(forMutating: program)
        var changed = false
        b.adopting {
            for instr in program.code {
                if adoptFixingBuiltinPath(instr, b) {
                    changed = true
                } else {
                    b.adopt(instr)
                }
            }
        }
        let fixed = b.finalize()
        return changed ? fixed : program
    }

    private static let allowedSimpleBuiltins: Set<String> = [
        "Mojo", "getOrCreatePrimaryRemote", "mojoAsyncFunctionPromises", "mojoSameOriginUrl",
        "globalThis",
        "ArrayBuffer", "DataView", "Int8Array", "Uint8Array", "Uint8ClampedArray",
        "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "BigInt64Array",
        "BigUint64Array", "Float16Array", "Float32Array", "Float64Array",
        "Array", "Object", "Map", "Set", "String", "Number", "Boolean",
        "Math", "Date", "RegExp", "Promise", "Error", "TypeError",
        "RangeError", "ReferenceError",
    ]

    /// If `instr` loads or calls a builtin with a dotted name as a member of
    /// another object, i.e. of `globalThis`, adopts it as a reference to the
    /// builtin itself and returns true. Also guards mutated property loads,
    /// element loads, method calls, 0-arg Remote/Router constructor mutations,
    /// and invalid `CreateNamedVariable` identifier mutations so a single mutated
    /// statement does not abort the rest of the program.
    private func adoptFixingBuiltinPath(_ instr: Instruction, _ b: ProgramBuilder) -> Bool {
        let environment = b.fuzzer.environment
        func isBuiltinPath(_ name: String) -> Bool {
            return name.contains(".") && environment.hasBuiltin(name)
        }

        if let op = instr.op as? CreateNamedVariable, op.declarationMode == .none,
            !isBuiltinPath(op.variableName) && !Self.allowedSimpleBuiltins.contains(op.variableName)
        {
            let output = b.adopt(instr.output)
            b.append(
                Instruction(
                    CreateNamedVariable("Object", declarationMode: .none), output: output))
            return true
        }

        if let op = instr.op as? GetProperty, isBuiltinPath(op.propertyName) {
            let output = b.adopt(instr.output)
            b.append(
                Instruction(
                    CreateNamedVariable(op.propertyName, declarationMode: .none),
                    output: output))
            return true
        }

        if let op = instr.op as? GetProperty, !op.isReceiverOptional {
            let output = b.adopt(instr.output)
            let input = b.adopt(instr.input(0))
            b.append(
                Instruction(
                    GetProperty(propertyName: op.propertyName, isReceiverOptional: true),
                    output: output, inputs: [input]))
            return true
        }

        if let op = instr.op as? GetElement, !op.isReceiverOptional {
            let output = b.adopt(instr.output)
            let input = b.adopt(instr.input(0))
            b.append(
                Instruction(
                    GetElement(index: op.index, isReceiverOptional: true),
                    output: output, inputs: [input]))
            return true
        }

        if instr.op is Construct || instr.op is ConstructWithSpread {
            let callee = b.adopt(instr.input(0))
            let group = b.type(of: callee).signature?.outputType.group ?? ""
            if (group.hasSuffix("Remote") || group.hasSuffix("CallbackRouter"))
                && instr.numInputs > 1
            {
                let output = b.adopt(instr.output)
                b.append(
                    Instruction(
                        Construct(numArguments: 0, isGuarded: false),
                        output: output, inputs: [callee]))
                return true
            }
        }

        if let op = instr.op as? CallMethod, isBuiltinPath(op.methodName) {
            let arguments = b.adopt(instr.inputs.dropFirst())
            let builtin = b.createNamedVariable(forBuiltin: op.methodName)
            let output = b.adopt(instr.output)
            let call: Operation =
                b.type(of: builtin).Is(.constructor())
                ? Construct(numArguments: arguments.count, isGuarded: false)
                : CallFunction(numArguments: arguments.count, isGuarded: false)
            b.append(Instruction(call, output: output, inputs: [builtin] + arguments))
            return true
        }

        if let op = instr.op as? CallMethod, !op.isReceiverOptional {
            let output = b.adopt(instr.output)
            let inputs = b.adopt(instr.inputs)
            b.append(
                Instruction(
                    CallMethod(
                        methodName: op.methodName, numArguments: op.numArguments,
                        isGuarded: op.isGuarded, isReceiverOptional: true,
                        isCallOptional: op.isCallOptional),
                    output: output, inputs: inputs))
            return true
        }

        return false
    }
}

/// WebIDL dictionaries (e.g. `MojoCreateDataPipeOptions`) are modeled as
/// `OptionsBag`s, which Fuzzilli emits as object literals.
///
/// Fuzzilli mutators randomly select instructions and variables to mutate. When
/// an object literal is selected, Fuzzilli searches for a `CodeGenerator`
/// registered for the `.objectLiteral` context. The builtin generators in this
/// context add properties or methods that are never used by the Mojo bindings.
///
/// However, `.objectLiteral` generators cannot simply be disabled: Fuzzilli
/// crashes if it fails to find an applicable generator for an active context.
/// Providing this no-op generator ensures Fuzzilli finds a valid generator to
/// run, which is cheaper than the builtin ones. All the other `.objectLiteral`
/// CodeGenerators are disabled.
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

private let mojoCommonStringNames = ["a", "b.txt", "sub", "db", "store", "idx", "lock", "test"]
private let mojoEdgeStringValues = [
    "", ".", "..", "a/b", "a\\\\b", ".crswap", "a..b", "c.exe", "d.html", "e.lnk",
    "a\\x00b", "\\x00", "sub\\x00.txt", "a\\r\\nb: c", "\\x01\\x1f\\x7f",
    ".git", ".ssh", ".well-known", "a/./b", "a/../b", "f.bat", "g.vbs", "h.sh",
    String(repeating: "a", count: 256),
    "txt", "html", "bin", "text/plain", "application/octet-stream",
    "https://a.test/a", "http://[::1]/",
]

/// Emits a string biased toward shared Mojo filenames/names and boundary inputs.
@discardableResult
public func emitMojoString(_ b: ProgramBuilder) -> Variable {
    let roll = Int.random(in: 0..<20)
    if roll < 9 {
        return b.loadString(chooseUniform(from: mojoCommonStringNames))
    } else if roll < 16 {
        return b.loadString(chooseUniform(from: mojoEdgeStringValues))
    } else {
        return b.loadString(b.randomString())
    }
}

/// Generates Mojo strings biased toward reusable filenames, directory/DB/lock
/// names, and path/protocol/NUL/CRLF boundary cases so repeated calls collide on
/// the same entries and exercise validators.
public let MojoStringGenerator = CodeGenerator(
    "MojoStringGenerator",
    produces: [.jsString]
) { b in
    emitMojoString(b)
}

private let mojoBoundaryIntValues: [Int64] = [
    0, 1, 2, 100, 101, 200, 204, 206, 304, 404, 500,
    -1, 255, 256, 4096, 65535, 65536, 0x7fff_ffff, -0x8000_0000,
]

private let mojoIntegerCustomNames: [String?] = [
    nil, nil, nil, "Uint8", "Int8", "Uint16", "Int16", "Uint32", "Int32",
]

/// Emits a numeric integer biased toward small counts, HTTP status codes
/// (`204`, `206`, `304`), and width/sign boundary values.
@discardableResult
public func emitMojoInteger(_ b: ProgramBuilder, as customName: String? = nil) -> Variable {
    let roll = Int.random(in: 0..<20)
    let val: Int64
    if roll < 9 {
        val = chooseUniform(from: [0, 1, 2, 4, 8, 16, 100, 200, 204, 206, 304])
    } else if roll < 16 {
        val = chooseUniform(from: mojoBoundaryIntValues)
    } else {
        val = b.randomInt()
    }
    if let name = customName {
        return b.loadInt(val, customName: name)
    }
    return b.loadInt(val)
}

/// Generates plain and named numeric integers (`Uint16`, `Int32`, etc.) biased
/// toward boundary counts, HTTP status codes, and limit edges.
public let MojoIntegerGenerator = CodeGenerator(
    "MojoIntegerGenerator",
    produces: [.integer, .jsInt8, .jsUint8, .jsInt16, .jsUint16, .jsInt32, .jsUint32]
) { b in
    emitMojoInteger(b, as: chooseUniform(from: mojoIntegerCustomNames))
}

private let mojoEdgeFloatValues: [Double] = [
    Double.nan, Double.infinity, -Double.infinity, -0.0,
    0.0, 1.0, -1.0, 0.5, -0.5,
    Double.greatestFiniteMagnitude, -Double.greatestFiniteMagnitude,
    Double.leastNonzeroMagnitude, -Double.leastNonzeroMagnitude,
]

/// Emits an IEEE-754 float/double biased toward non-finite, `-0.0`, and extreme
/// boundary values alongside small finite numbers.
@discardableResult
public func emitMojoFloat(_ b: ProgramBuilder) -> Variable {
    let roll = Int.random(in: 0..<20)
    let val: Double
    if roll < 9 {
        val = chooseUniform(from: [Double.nan, Double.infinity, -Double.infinity, -0.0])
    } else if roll < 16 {
        val = chooseUniform(from: mojoEdgeFloatValues)
    } else {
        val = b.randomFloat()
    }
    return b.loadFloat(val)
}

/// Generates IEEE-754 boundary floats (`NaN`, `±Infinity`, `-0.0`, extremes)
/// for Mojo double/float fields and key comparisons.
public let MojoFloatGenerator = CodeGenerator(
    "MojoFloatGenerator",
    produces: [.float]
) { b in
    emitMojoFloat(b)
}

/// Emits a BigInt biased toward valid offsets, sizes, capacity deltas, and
/// boundary integers.
@discardableResult
public func emitMojoBigInt(_ b: ProgramBuilder) -> Variable {
    let roll = Int.random(in: 0..<20)
    let val: Int64
    if roll < 11 {
        val = chooseUniform(from: [0, 1, 16, 256, 4096, 32768])
    } else if roll < 16 {
        val = chooseUniform(from: [-1, -16, -256, -4096])
    } else {
        val = chooseUniform(from: [0x7fff_ffff, Int64.max, Int64.min])
    }
    return b.loadBigInt(val)
}

/// Generates BigInt offsets, sizes, and versions in scope so uint64/int64 Mojo
/// parameters frequently receive valid small values alongside boundary integers.
public let MojoBigIntGenerator = CodeGenerator(
    "MojoBigIntGenerator",
    produces: [.bigint]
) { b in
    emitMojoBigInt(b)
}
