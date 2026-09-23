// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for `skia.mojom` types.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let skiaMojomBitmapN32ImageInfo = "skia.mojom.BitmapN32ImageInfo"
    static let skiaMojomAlphaType = "skia.mojom.AlphaType"
}

public let skiaMojomBuiltins: [String: ILType] = [
    CommonMojoStrings.skiaMojomBitmapN32ImageInfo: .jsSkiaMojomBitmapN32ImageInfoConstructor
]

public let skiaMojomCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoSkiaMojomColorTransferFunctionArrayGenerator, 1),
    (MojoSkiaMojomColorToXyzMatrixArrayGenerator, 1),
]

public let skiaMojomObjectGroups: [ObjectGroup] = [
    .skiaMojomBitmapN32ImageInfo
]

public let skiaMojomEnumerations: [ILType] = [
    .jsSkiaMojomAlphaType
]

extension ILType {
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
}

extension ObjectGroup {
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
}

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
