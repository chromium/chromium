// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for `webnn.mojom` types.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let webnnMojomDataType = "webnn.mojom.DataType"
    static let webnnMojomDevice = "webnn.mojom.Device"
    static let webnnMojomOperandDescriptor = "webnn.mojom.OperandDescriptor"
}

public let webnnMojomBuiltins: [String: ILType] = [
    CommonMojoStrings.webnnMojomOperandDescriptor: .jsWebnnMojomOperandDescriptorConstructor,
]

public let webnnMojomCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoWebnnMojomOperandDescriptorGenerator, 1),
]

public let webnnMojomObjectGroups: [ObjectGroup] = [
    .webnnMojomOperandDescriptor,
]

public let webnnMojomEnumerations: [ILType] = [
    .jsWebnnMojomDataType,
    .jsWebnnMojomDevice,
]

extension ILType {
    public static let jsWebnnMojomDataType: ILType = .intEnumeration(
        ofName: CommonMojoStrings.webnnMojomDataType, withValues: Array(0...9)
    )
    /// Only `kGpu` (1). `WebNNContextProviderImpl::CreateWebNNContext` answers
    /// `kCpu` (0) and `kNpu` (2) with a `kFallbackToInProcess` error, as the
    /// renderer is expected to run these devices in-process. These values never
    /// reach the rest of the WebNN service, so fuzzing them is wasted effort.
    public static let jsWebnnMojomDevice: ILType = .intEnumeration(
        ofName: CommonMojoStrings.webnnMojomDevice, withValues: [1]
    )
    public static let jsWebnnMojomOperandDescriptor: ILType = .object(
        ofGroup: CommonMojoStrings.webnnMojomOperandDescriptor,
        withProperties: ["dataType", "shape", "pendingPermutation"]
    )
    public static let jsWebnnMojomOperandDescriptorConstructor: ILType = .constructor(
        [
            .plain(.jsWebnnMojomDataType),
            .plain(.createJsArrayType(ofElementType: .jsUint32)),
            .plain(.createJsArrayType(ofElementType: .jsUint32)),
        ] => .jsWebnnMojomOperandDescriptor
    )
}

extension ObjectGroup {
    public static let webnnMojomOperandDescriptor = ObjectGroup(
        name: CommonMojoStrings.webnnMojomOperandDescriptor,
        instanceType: .jsWebnnMojomOperandDescriptor,
        properties: [
            "dataType": .jsWebnnMojomDataType,
            "shape": .createJsArrayType(ofElementType: .jsUint32),
            "pendingPermutation": .createJsArrayType(ofElementType: .jsUint32),
        ],
        methods: [:]
    )
}

/// Generates an `OperandDescriptor` that passes
/// `OperandDescriptor::CreateForDeserialization` in the WebNN service. Arguments
/// generated for the constructor are almost never valid: the shape and the
/// pending permutation, which have the same type, are often the same array, so
/// the permutation is invalid, and random dimensions overflow the byte length.
///
/// The descriptor has:
/// - a rank of 0 to 4, with dimensions of 1 to 8, far below the limits on the
///   rank (8), the dimensions and the number of elements (int32_t), and the
///   byte length of tensors;
/// - a pending permutation that is empty, or a permutation of the dimensions.
///   Sub-byte data types (`kInt4`, `kUint4`) do not support permutations.
public let MojoWebnnMojomOperandDescriptorGenerator = CodeGenerator(
    "MojoWebnnMojomOperandDescriptorGenerator",
    produces: [.jsWebnnMojomOperandDescriptor]
) { b in
    let dataType = Int64.random(in: 0...9)
    let isSubByte = dataType >= 8
    let rank = Int64.random(in: 0...4)

    let shape = (0..<rank).map { _ in b.loadInt(Int64.random(in: 1...8)) }
    var pendingPermutation: [Variable] = []
    if !isSubByte && probability(0.25) {
        pendingPermutation = (0..<rank).shuffled().map { b.loadInt($0) }
    }

    let constructor = b.createNamedVariable(
        forBuiltin: CommonMojoStrings.webnnMojomOperandDescriptor)
    b.construct(
        constructor,
        withArgs: [
            b.loadInt(dataType), b.createArray(with: shape),
            b.createArray(with: pendingPermutation),
        ])
}
