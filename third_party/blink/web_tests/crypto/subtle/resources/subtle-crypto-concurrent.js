if (self.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('common.js');
}

function shouldEvaluateAsSilent(expressionToEval, expectedResult)
{
    var result = eval(expressionToEval);
    if (result !== expectedResult) {
        testFailed(expressionToEval + " evaluated to " + result + " instead of " + expectedResult);
    }
}

function doPostMessage(data)
{
    if (isWorker())
        self.postMessage(data);
    else
        self.postMessage(data, '*');
}

function notifySuccess()
{
    doPostMessage("TEST_FINISHED");
}

function notifyFailure(details)
{
    doPostMessage("FAIL:" + details);
}

function testGenerateRsaKey()
{
    var extractable = false;
    var usages = ['sign', 'verify'];
    var algorithm = {name: "RSASSA-PKCS1-v1_5", modulusLength: 2048, publicExponent: hexStringToUint8Array("010001"), hash: {name: "SHA-256"}};

    return crypto.subtle.generateKey(algorithm, extractable, usages).then(function(result) {
        publicKey = result.publicKey;
        privateKey = result.privateKey;

        shouldBeTrue("publicKey instanceof CryptoKey", true);

        shouldEvaluateAsSilent("publicKey.type", "public");
        shouldEvaluateAsSilent("publicKey.extractable", true);
        shouldEvaluateAsSilent("publicKey.algorithm.name", algorithm.name);
        shouldEvaluateAsSilent("publicKey.algorithm.modulusLength", algorithm.modulusLength);
        shouldEvaluateAsSilent("bytesToHexString(publicKey.algorithm.publicExponent)", "010001");

        shouldEvaluateAsSilent("privateKey.type", "private");
        shouldEvaluateAsSilent("privateKey.extractable", false);
        shouldEvaluateAsSilent("privateKey.algorithm.name", algorithm.name);
        shouldEvaluateAsSilent("privateKey.algorithm.modulusLength", algorithm.modulusLength);
        shouldEvaluateAsSilent("bytesToHexString(privateKey.algorithm.publicExponent)", "010001");
    });
}

// Very similar to "hmac-sign-verify.html".
function testHmac()
{
    var importAlgorithm = {name: 'HMAC', hash: {name: "SHA-256"}};
    var algorithm = {name: 'HMAC'};

    var key = null;

    var testCase = {
      hash: "SHA-256",
      key: "9779d9120642797f1747025d5b22b7ac607cab08e1758f2f3a46c8be1e25c53b8c6a8f58ffefa176",
      message: "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e",
      mac: "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b"
    };

    var keyData = hexStringToUint8Array(testCase.key);
    var usages = ['sign', 'verify'];
    var extractable = true;

    // (1) Import the key
    return crypto.subtle.importKey('raw', keyData, importAlgorithm, extractable, usages).then(function(result) {
        key = result;

        // shouldBe() can only resolve variables in global context.
        tmpKey = key;
        shouldEvaluateAsSilent("tmpKey.type", "secret");
        shouldEvaluateAsSilent("tmpKey.extractable", true);
        shouldEvaluateAsSilent("tmpKey.algorithm.name", "HMAC");
        shouldEvaluateAsSilent("tmpKey.algorithm.hash.name", testCase.hash);
        shouldEvaluateAsSilent("tmpKey.algorithm.length", keyData.length * 8);
        shouldEvaluateAsSilent("tmpKey.usages.join(',')", "sign,verify");

        // (2) Sign.
        var signPromise = crypto.subtle.sign(algorithm, key, hexStringToUint8Array(testCase.message));

        // (3) Verify
        var verifyPromise = crypto.subtle.verify(algorithm, key, hexStringToUint8Array(testCase.mac), hexStringToUint8Array(testCase.message));

        // (4) Verify truncated mac (by stripping 1 byte off of it).
        var expectedMac = hexStringToUint8Array(testCase.mac);
        var verifyTruncatedPromise = crypto.subtle.verify(algorithm, key, expectedMac.subarray(0, expectedMac.byteLength - 1), hexStringToUint8Array(testCase.message));

        var exportKeyPromise = crypto.subtle.exportKey('raw', key);

        return Promise.all([signPromise, verifyPromise, verifyTruncatedPromise, exportKeyPromise]);
    }).then(function(result) {
        // signPromise
        mac = result[0];
        shouldEvaluateAsSilent("bytesToHexString(mac)", testCase.mac);

        // verifyPromise
        verifyResult = result[1];
        shouldEvaluateAsSilent("verifyResult", true);

        // verifyTruncatedPromise
        verifyResult = result[2];
        shouldEvaluateAsSilent("verifyResult", false);

        // exportKeyPromise
        exportedKeyData = result[3];
        shouldEvaluateAsSilent("bytesToHexString(exportedKeyData)", testCase.key);
    });
}

// Very similar to aes-gcm-encrypt-decrypt.hml
function testAesGcm()
{
    var testCase = {
      "key": "e03548984a7ec8eaf0870637df0ac6bc17f7159315d0ae26a764fd224e483810",
      "iv": "f4feb26b846be4cd224dbc5133a5ae13814ebe19d3032acdd3a006463fdb71e83a9d5d96679f26cc1719dd6b4feb3bab5b4b7993d0c0681f36d105ad3002fb66b201538e2b7479838ab83402b0d816cd6e0fe5857e6f4adf92de8ee72b122ba1ac81795024943b7d0151bbf84ce87c8911f512c397d14112296da7ecdd0da52a",
      "cipherText": "fda718aa1ec163487e21afc34f5a3a34795a9ee71dd3e7ee9a18fdb24181dc982b29c6ec723294a130ca2234952bb0ef68c0f3",
      "additionalData": "aab26eb3e7acd09a034a9e2651636ab3868e51281590ecc948355e457da42b7ad1391c7be0d9e82895e506173a81857c3226829fbd6dfb3f9657a71a2934445d7c05fa9401cddd5109016ba32c3856afaadc48de80b8a01b57cb",
      "authenticationTag": "4795fbe0",
      "plainText": "69fd0c9da10b56ec6786333f8d76d4b74f8a434195f2f241f088b2520fb5fa29455df9893164fb1638abe6617915d9497a8fe2"
    }

    var key = null;
    var keyData = hexStringToUint8Array(testCase.key);
    var iv = hexStringToUint8Array(testCase.iv);
    var additionalData = hexStringToUint8Array(testCase.additionalData);
    var tag = hexStringToUint8Array(testCase.authenticationTag);
    var usages = ['encrypt', 'decrypt'];
    var extractable = false;

    var tagLengthBits = tag.byteLength * 8;

    var algorithm = {name: 'aes-gcm', iv: iv, additionalData: additionalData, tagLength: tagLengthBits};

    // (1) Import the key
    return crypto.subtle.importKey('raw', keyData, algorithm, extractable, usages).then(function(result) {
        key = result;

        // shouldBe() can only resolve variables in global context.
        tmpKey = key;
        shouldEvaluateAsSilent("tmpKey.type", "secret");
        shouldEvaluateAsSilent("tmpKey.extractable", false);
        shouldEvaluateAsSilent("tmpKey.algorithm.name", "AES-GCM");
        shouldEvaluateAsSilent("tmpKey.usages.join(',')", "encrypt,decrypt");

        // (2) Encrypt
        var encryptPromise1 = crypto.subtle.encrypt(algorithm, key, hexStringToUint8Array(testCase.plainText));
        var encryptPromise2 = crypto.subtle.encrypt(algorithm, key, hexStringToUint8Array(testCase.plainText));

        // (3) Decrypt
        var decryptPromise1 = crypto.subtle.decrypt(algorithm, key, hexStringToUint8Array(testCase.cipherText + testCase.authenticationTag));
        var decryptPromise2 = crypto.subtle.decrypt(algorithm, key, hexStringToUint8Array(testCase.cipherText + testCase.authenticationTag));

        return Promise.all([encryptPromise1, encryptPromise2, decryptPromise1, decryptPromise2]);
    }).then(function(result) {
        // encryptPromise1, encryptPromise2
        for (var i = 0; i < 2; ++i) {
            cipherText = result[i];
            shouldEvaluateAsSilent("bytesToHexString(cipherText)", testCase.cipherText + testCase.authenticationTag);
        }

        // decryptPromise1, decryptPromise2
        for (var i = 0; i < 2; ++i) {
            plainText = result[2 + i];
            shouldEvaluateAsSilent("bytesToHexString(plainText)", testCase.plainText);
        }
    });
}

Promise.all([
    testHmac(),
    testGenerateRsaKey(),
    testAesGcm(),
    testHmac(),
    testAesGcm(),
]).then(notifySuccess, notifyFailure);
