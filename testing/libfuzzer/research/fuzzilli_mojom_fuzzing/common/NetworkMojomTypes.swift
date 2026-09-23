// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for `network.mojom` types.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let networkMojomConnectionInfo = "network.mojom.ConnectionInfo"
    static let networkMojomEffectiveConnectionType = "network.mojom.EffectiveConnectionType"
}

public let networkMojomEnumerations: [ILType] = [
    .jsNetworkMojomConnectionInfo,
    .jsNetworkMojomEffectiveConnectionType,
]

extension ILType {
    public static let jsNetworkMojomConnectionInfo: ILType = .intEnumeration(
        ofName: CommonMojoStrings.networkMojomConnectionInfo, withValues: Array(0...42)
    )
    public static let jsNetworkMojomEffectiveConnectionType: ILType = .intEnumeration(
        ofName: CommonMojoStrings.networkMojomEffectiveConnectionType, withValues: Array(0...5)
    )
}
