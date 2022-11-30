// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
const char kTestMessage[] = "Test Message";

// |kTestMessage| signed with the key from |kTestRsaKeyPair|.
const char kExpectedSignature[] =
"LfUyXU2AiKM4rpWivUR3bLiQiRt1W3iIenNfJEB8RWyoEfnvSBoD52x8q9yFvtLFDEMPWyIrwM+N2"
"LuaWBKG1c0R7h+twBgvpExzZneJl+lbGMRx9ba8m/KAFrUWA/NRzOen2NHCuPybOEasgrPgGWBrmf"
"gDcvyW8QiGuKLopGj/4c5CQT4yE8JjsyU3Qqo2ZPK4neJYQhOmAlg+Q5dAPLpzWMj5HQyOVHJaSXZ"
"Y8vl/LiKvbdofYLeYNVKAE4q5mfpQMrsysPYpbxBV60AhFyrvtC040MFGcflKQRZNiZwMXVb7DclC"
"BPgvK7rI5Y0ERtVm+yNmH7vCivfyAnDUYA==";

// Another RSA key pair, different from |kTestRsaKeyPair|
const char kTestRsaKeyPair2[] =
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDLNd9bNNBFxXSasqWHv8ydItmoi"
"NhiWV/1Z6JI6TpKTnTm9yQSxIoCLHT8fB8QA0wzzgYEuZpDHIZDv1WxSdC3kgx84JrWe43+SPDZ2b"
"7ekuhXinPK+f3Nw5GjGaAUevyeWNoDD3GFka5q4zw8W1OE8E/z50FfPqOUejg+qyrgovEWQIZWvI0"
"CHdy8HTtxT7G0YbPJYZ8ycRkUrsXtY2RQX4IaCMEcdAmCm2Q2hYldulbZX6Bvv5GX8FOWYOerbYVK"
"ZsmH5II+KEP4We75ONVR2jdCCJ5L3YMXtbYtZZy1yZXcOf2fpBo6+p81M7rG9com/C75QoMVMVJJC"
"ahCli7/AgMBAAECggEAZNzThUC8k7UDQHmlgbCojeIraOSrin1UDMmomknxHcq9aZqHtC0LVzLbyi"
"qNfHQ2kYwUHqpFMERrPBsvHHVH/KWoPx94mzbubqWjrm3OuEjwu+rDuJ7G5CfLFMp2U1QMKUhuxZA"
"Xx7Vcfj9VuZuW4+gntyc0omLD7MGRQ0HQYXh7ZDGWrMPEs6Cjzcx9/G9AD7ysWIqk14iwJqKhztiD"
"NirMr64eDZFzzDvXTl3j5l+yiAHiV5LPUUKyCe+jEdZMceSKy5wSZXSkiW4zhgEzwdMN2zmxlcC59"
"17dw2c6xD+tKxTMwzx77sauBFNzebNU1m5hIKH+jCPiA8aQv3/l8QKBgQD8J4ilT/CV6hhw0/q99e"
"+54s+SIz8nYo8fvyeEBCiA4lf/OZuxl/sYWK+PthP+xzsjKTq7yoFkliXrtOioW34E0WdLv+6jEXQ"
"hxaXZyk0TMwxm228xMG66evXDJ8OGWCi6uiAnVWNUu6VXacQwKHf9hv6DNRNcmkQGojfcx3ZNZwKB"
"gQDOT0ApweRr0zA28l8C6qtNfY/NIeWTWPDTspt8zDoX1ILXc2HZHYW0QtUQ6tXNQnUW/ymavla7E"
"upa8AoZoqMvaUIg1BjMYIzc4yQMVf1BRCfvT8GsoymX+8Gt/DB34L7KMPOhQysMmZMNjKtozQsbZY"
"wcnN19pWnDv78trFZ6qQKBgHy1nMqN7+JlRjM/VCrxYOAhwiF31ztGbpz38LZFTDb6OyVau5spHKH"
"c8u9z0Q3YQXJRaOAJ9tblv9mEvvDNV1VQr/Lx+TejYTl2xGEjwdz2CXMxohvE5W7Lc5NSrkxae8Jm"
"XZK2k4sLx2mlQMfErBuy0VvZOzs4fN5/CnviFquPAoGBAI8NAI5ztPDW1L2kvSCGmxT2FTnFYSwUJ"
"ZiEZa/Y5AcWAUtm49fp0oW1OYuraWgTxqCVeMGlbPn2Ga3IdxhjXwdG0uV0a2V7JPEcRiiPjzUsDw"
"yunroXwIVzuU3saacVnPURkDynGDh6XC6u9UOLuUHb3ZURZ7rxcS6by/HdZ3FRAoGBAOhjtjyfFEn"
"bZtjcQd+bNtoTPV/L71+K8AYPwV0td5Qy5VbBrTIlv7pNFxE4bYNuEe6cI2cxTua4i5IoKYXyUm5u"
"SvUVkkz7CpoiFwMnnLsNrZmazVS2zq0Y2a2ai8C3mPgLOdroS2fBBAcuFApeq1PvISmT6ZnJJ8Yah"
"HQCfClh";

}  // namespace

class RsaKeyPairTest : public testing::Test {
};

TEST_F(RsaKeyPairTest, ImportExportImport) {
  // Load a key from a string, export to a string, load again, and verify that
  // we generate the same signature with both keys.
  scoped_refptr<RsaKeyPair> exported_key = RsaKeyPair::FromString(
      kTestRsaKeyPair);
  scoped_refptr<RsaKeyPair> imported_key = RsaKeyPair::FromString(
      exported_key->ToString());

  ASSERT_EQ(exported_key->SignMessage(kTestMessage),
            imported_key->SignMessage(kTestMessage));
}

TEST_F(RsaKeyPairTest, Signatures) {
  // Sign a message and check that we get the expected signature.
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);

  std::string signature_base64 = key_pair->SignMessage(kTestMessage);
  ASSERT_EQ(signature_base64, std::string(kExpectedSignature));
}

TEST_F(RsaKeyPairTest, GenerateKey) {
  // Test that we can generate a valid key.
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair.get());
  ASSERT_NE(key_pair->ToString(), "");
  ASSERT_NE(key_pair->GetPublicKey(), "");
  ASSERT_NE(key_pair->SignMessage(kTestMessage), "");
}

TEST_F(RsaKeyPairTest, SignaturesDiffer) {
  // Sign using different keys/messages and check that signatures are different.
  scoped_refptr<RsaKeyPair> key_pair1 = RsaKeyPair::FromString(kTestRsaKeyPair);
  scoped_refptr<RsaKeyPair> key_pair2 = RsaKeyPair::FromString(
      kTestRsaKeyPair2);

  std::string signature_kp1_msg1_base64 = key_pair1->SignMessage(kTestMessage);
  std::string signature_kp2_msg1_base64 = key_pair2->SignMessage(kTestMessage);
  std::string signature_kp1_msg2_base64 = key_pair1->SignMessage("Different");
  ASSERT_NE(signature_kp1_msg1_base64, signature_kp2_msg1_base64);
  ASSERT_NE(signature_kp1_msg1_base64, signature_kp1_msg2_base64);
  ASSERT_NE(key_pair1->GetPublicKey(), key_pair2->GetPublicKey());
}

}  // namespace remoting
