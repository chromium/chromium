// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Definitions for `url.mojom` types.
///
/// See `MojoCommonProfile.swift` for the naming conventions these definitions
/// follow and for how the lists below are registered.

extension CommonMojoStrings {
    static let urlMojomUrl = "url.mojom.Url"
    static let urlMojomSchemeHostPort = "url.mojom.SchemeHostPort"
}

public let urlMojomBuiltins: [String: ILType] = [
    CommonMojoStrings.urlMojomSchemeHostPort: .jsUrlMojomSchemeHostPortConstructor,
    CommonMojoStrings.urlMojomUrl: .jsUrlMojomUrlConstructor,
]

public let urlMojomCodeGenerators: [(CodeGenerator, Int)] = [
    (MojoUrlMojomUrlSchemeHostPortGenerator, 1),
    (MojoUrlMojomUrlGenerator, 1),
]

public let urlMojomObjectGroups: [ObjectGroup] = [
    .urlMojomSchemeHostPort,
    .urlMojomUrl,
]

extension ILType {
    public static let jsUrlMojomUrl: ILType = .object(
        ofGroup: CommonMojoStrings.urlMojomUrl, withProperties: ["url"])
    public static let jsUrlMojomUrlConstructor: ILType = .constructor(
        [.string] => .jsUrlMojomUrl
    )
    public static let jsUrlMojomSchemeHostPort: ILType = .object(
        ofGroup: CommonMojoStrings.urlMojomSchemeHostPort,
        withProperties: ["scheme", "host", "port"])
    public static let jsUrlMojomSchemeHostPortConstructor: ILType = .constructor(
        [.string, .string, .integer] => .jsUrlMojomSchemeHostPort
    )
}

extension ObjectGroup {
    public static let urlMojomUrl = ObjectGroup(
        name: CommonMojoStrings.urlMojomUrl,
        instanceType: .jsUrlMojomUrl,
        properties: [
            "url": .string
        ],
        methods: [:]
    )
    public static let urlMojomSchemeHostPort = ObjectGroup(
        name: CommonMojoStrings.urlMojomSchemeHostPort,
        instanceType: .jsUrlMojomSchemeHostPort,
        properties: [
            "scheme": .string,
            "host": .string,
            "port": .integer,
        ],
        methods: [:]
    )
}

// TODO(http://crbug.com/514397167) determine broader URL generation strategy
public let MojoUrlMojomUrlGenerator = CodeGenerator(
    "MojoUrlMojomUrlGenerator",
    produces: [.jsUrlMojomUrl]
) { b in
    let urlString = b.loadString("https://example.com/" + b.randomString())
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.urlMojomUrl)
    b.construct(constructor, withArgs: [urlString])
}

// TODO(http://crbug.com/514397167) determine broader SchemeHostPort generation strategy
public let MojoUrlMojomUrlSchemeHostPortGenerator = CodeGenerator(
    "MojoUrlMojomUrlSchemeHostPortGenerator",
    produces: [.jsUrlMojomSchemeHostPort]
) { b in
    let schemes = ["https", "wss"]
    let hosts = ["example.com", "localhost", "127.0.0.1", "[::1]", "xn--n3h.net"]

    let selectedScheme = chooseUniform(from: schemes)
    let selectedHost = chooseUniform(from: hosts)

    let port = Int64.random(in: 1...65535)

    let schemeVar = b.loadString(selectedScheme)
    let hostVar = b.loadString(selectedHost)
    let portVar = b.loadInt(port)

    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.urlMojomSchemeHostPort)
    b.construct(constructor, withArgs: [schemeVar, hostVar, portVar])
}
