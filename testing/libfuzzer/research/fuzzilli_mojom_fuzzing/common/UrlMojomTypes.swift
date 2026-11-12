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

private let mojoSameOriginPaths = ["/", "/a", "/b.txt", "/index.html", "/gen/file.txt", "/test?x=1#frag"]
private let mojoStaticUrls = [
    "https://a.test/a", "https://localhost/", "http://127.0.0.1/",
    "wss://a.test/", "about:blank", "data:text/plain,a", "",
]

public let MojoUrlMojomUrlGenerator = CodeGenerator(
    "MojoUrlMojomUrlGenerator",
    produces: [.jsUrlMojomUrl]
) { b in
    let urlString: Variable
    let roll = Int.random(in: 0..<10)
    if roll < 4 {
        let sameOriginFn = b.createNamedVariable(forBuiltin: "mojoSameOriginUrl")
        let pathVar = b.loadString(chooseUniform(from: mojoSameOriginPaths))
        urlString = b.callFunction(sameOriginFn, withArgs: [pathVar])
        b.hide(pathVar)
        b.hide(sameOriginFn)
    } else if roll < 7 {
        urlString = b.loadString(chooseUniform(from: mojoStaticUrls))
    } else {
        urlString = b.loadString("https://example.com/" + b.randomString())
    }
    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.urlMojomUrl)
    b.construct(constructor, withArgs: [urlString])
    b.hide(constructor)
}

public let MojoUrlMojomUrlSchemeHostPortGenerator = CodeGenerator(
    "MojoUrlMojomUrlSchemeHostPortGenerator",
    produces: [.jsUrlMojomSchemeHostPort]
) { b in
    let schemes = ["https", "wss", "http", "file", "about"]
    let hosts = ["a.test", "example.com", "localhost", "127.0.0.1", "[::1]", "xn--n3h.net"]

    let selectedScheme = chooseUniform(from: schemes)
    let selectedHost = chooseUniform(from: hosts)

    let port: Int64 =
        probability(0.5)
        ? chooseUniform(from: [0, 80, 443, 8080, 65535])
        : Int64.random(in: 1...65535)

    let schemeVar = b.loadString(selectedScheme)
    let hostVar = b.loadString(selectedHost)
    let portVar = b.loadInt(port)

    let constructor = b.createNamedVariable(forBuiltin: CommonMojoStrings.urlMojomSchemeHostPort)
    b.construct(constructor, withArgs: [schemeVar, hostVar, portVar])
    b.hide(constructor)
}
