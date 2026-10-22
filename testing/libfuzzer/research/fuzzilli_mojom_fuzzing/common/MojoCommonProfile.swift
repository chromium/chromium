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

public let commonMojoBuiltins: [String: ILType] = [:]
    .merging(mojoCoreBuiltins) { (existing, _) in existing }
    .merging(mojoBaseMojomBuiltins) { (existing, _) in existing }
    .merging(skiaMojomBuiltins) { (existing, _) in existing }
    .merging(urlMojomBuiltins) { (existing, _) in existing }

public let commonMojoCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoAsyncFunctionGenerator, 5000),
    (MojoAwaitGenerator, 5000),
    (MojoObjectLiteralNoopGenerator, 1),
    (MojoBufferSourceGenerator, 1),
]
    + mojoBaseMojomCodeGenerators
    + skiaMojomCodeGenerators
    + urlMojomCodeGenerators

public let commonMojoObjectGroups: [ObjectGroup] = []
    + mojoCoreObjectGroups
    + mojoBaseMojomObjectGroups
    + skiaMojomObjectGroups
    + urlMojomObjectGroups

public let commonMojoEnumerations: [ILType] = []
    + networkMojomEnumerations
    + skiaMojomEnumerations

public let commonMojoOptionsBags: [OptionsBag] = []
    + mojoCoreOptionsBags

extension ILType {
    // TODO(crbug.com/554102710): Upstream `jsBufferSource` to be a builtin type in Fuzzilli
    public static let jsBufferSource: ILType =
        JavaScriptEnvironment.typedArrayConstructors
        .map { jsTypedArray($0) }
        .reduce(.jsArrayBuffer | .jsDataView, |)
}

/// Generate large async functions where Fuzzilli can await Mojo calls and use
/// the return values in interesting ways. To avoid many nested functions, the
/// generator only emits one instruction when called in an `.async` context.
///
/// The promise returned by each async function is pushed to
/// `mojoAsyncFunctionPromises`, which every profile declares in `codePrefix`
/// and awaits in `codeSuffix`, so that the function runs to completion.
public let MojoAsyncFunctionGenerator = CodeGenerator(
    "MojoAsyncFunctionGenerator"
) { b in
    if b.context.contains(.async) {
        b.build(n: 1)
        return
    }

    let asyncFunction = b.buildAsyncArrowFunction(with: .parameters(n: 0)) { _ in
        b.build(n: 40)
    }
    let promise = b.callFunction(asyncFunction, withArgs: [])
    let promises = b.createNamedVariable(forBuiltin: "mojoAsyncFunctionPromises")
    b.callMethod("push", on: promises, withArgs: [promise])
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
