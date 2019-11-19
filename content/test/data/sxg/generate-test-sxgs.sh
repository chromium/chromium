#!/bin/sh

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
variants_header=variants-04
variant_key_header=variant-key-04
signature_date=2019-07-28T00:00:00Z

set -e

for cmd in gen-signedexchange gen-certurl dump-signedexchange; do
    if ! command -v $cmd > /dev/null 2>&1; then
        echo "$cmd is not installed. Please run:"
        echo "  go get -u github.com/WICG/webpackage/go/signedexchange/cmd/..."
        exit 1
    fi
done

dumpSignature() {
  echo "constexpr char $1[] = R\"($(dump-signedexchange -signature -i $2))\";"
}

tmpdir=$(mktemp -d)
sctdir=$tmpdir/scts
mkdir $sctdir

# Make dummy OCSP and SCT data for cbor certificate chains.
echo -n OCSP >$tmpdir/ocsp; echo -n SCT >$sctdir/dummy.sct

# Generate the certificate chain of "*.example.org", whose validity period is
# exactly 90 days.
gen-certurl -pem prime256v1-sha256.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir > test.example.org.public.pem.cbor

# Generate the certificate chain of "*.example.org", whose validity period is
# more than 90 days.
gen-certurl -pem prime256v1-sha256-validity-too-long.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir \
  > test.example.org-validity-too-long.public.pem.cbor

# Generate the certificate chain of "*.example.org", without
# CanSignHttpExchangesDraft extension.
gen-certurl -pem prime256v1-sha256-noext.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir > test.example.org-noext.public.pem.cbor

# Generate the certificate chain of "*.example.org", for
# SignedExchangeRequestHandlerRealCertVerifierBrowserTest.
gen-certurl -pem prime256v1-sha256-long-validity.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir \
  > test.example.org-long-validity.public.pem.cbor

# Generate the signed exchange file.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -o test.example.org_test.sxg \
  -miRecordSize 100

# Generate the signed exchange for the missing nosniff header test case.
cp test.example.org_test.sxg test.example.org_test_missing_nosniff.sxg

# Generate the signed exchange for the invalid content-type test case.
cp test.example.org_test.sxg test.example.org_test_invalid_content_type.sxg

# Generate the signed exchange for downloading test case.
cp test.example.org_test.sxg test.example.org_test_download.sxg

# Generate the signed exchange file with invalid magic string
xxd -p test.example.org_test.sxg |
  sed '1s/^737867312d62..00/737867312d787800/' |
  xxd -r -p > test.example.org_test_invalid_magic_string.sxg

# Generate the signed exchange file with invalid cbor header.
# 0xa4 : start map of 4 element -> 0xa5 : 5 elements.
xxd -p test.example.org_test.sxg |
  tr -d '\n' |
  sed 's/a44664/a54664/' |
  xxd -r -p > test.example.org_test_invalid_cbor_header.sxg

# Generate the signed exchange file with bad MICE integrity.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content badmice_test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -o - \
  -miRecordSize 32 |
  xxd -p |
  tr -d '\n' |
  sed 's/585858/4f4f4f/' |
  xxd -r -p > test.example.org_test_bad_mice.sxg

# Generate the signed exchange file with bad MICE integrity (small).
# s/Loc/OOO/
xxd -p test.example.org_test.sxg |
  tr -d '\n' |
  sed 's/4c6f63/4f4f4f/' |
  xxd -r -p > test.example.org_test_bad_mice_small.sxg

# Generate the signed exchange file with noext certificate
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256-noext.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -o test.example.org_noext_test.sxg \
  -miRecordSize 100

# Generate the signed exchange file whose certificate's validity period is more
# than 90 days.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256-validity-too-long.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -o test.example.org_cert_validity_too_long.sxg \
  -miRecordSize 100

# Generate the signed exchange file for
# SignedExchangeRequestHandlerRealCertVerifierBrowserTest.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256-long-validity.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -o test.example.org_long_cert_validity.sxg \
  -miRecordSize 100

# Generate the signed exchange file with invalid URL.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.com/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.com/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -o test.example.com_invalid_test.sxg \
  -miRecordSize 100

# Generate the signed exchange for a plain text file.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/hello.txt \
  -status 200 \
  -content hello.txt \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -responseHeader 'Content-Type: text/plain; charset=iso-8859-1' \
  -date $signature_date \
  -expire 168h \
  -o test.example.org_hello.txt.sxg

# Generate the signed exchange whose content is gzip-encoded.
gzip -c test.html >$tmpdir/test.html.gz
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content $tmpdir/test.html.gz \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -responseHeader 'Content-Encoding: gzip' \
  -date $signature_date \
  -expire 168h \
  -o test.example.org_test.html.gz.sxg

# Generate the signed exchange with variants / variant-key headers.
gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -responseHeader "${variants_header}: accept-language;en;fr" \
  -responseHeader "${variant_key_header}: fr" \
  -o test.example.org_fr_variant.sxg \
  -miRecordSize 100

echo "Update the test signatures in "
echo "signed_exchange_signature_verifier_unittest.cc with the followings:"
echo "===="

gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate ./prime256v1-sha256.public.pem \
  -privateKey ./prime256v1.key \
  -date 2018-02-06T04:45:41Z \
  -validityUrl https://test.example.org/resource.validity.msg \
  -o $tmpdir/out.htxg \
  -dumpHeadersCbor $tmpdir/out.cborheader

dumpSignature kSignatureHeaderECDSAP256 $tmpdir/out.htxg

echo 'constexpr uint8_t kCborHeadersECDSAP256[] = {'
xxd --include $tmpdir/out.cborheader | sed '1d;$d'

gen-signedexchange \
  -version 1b3 \
  -uri https://test.example.org/test/ \
  -validityUrl https://test.example.org/resource.validity.msg \
  -content test.html \
  -certificate ./secp384r1-sha256.public.pem \
  -privateKey ./secp384r1.key \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg

dumpSignature kSignatureHeaderECDSAP384 $tmpdir/out.htxg

echo "===="

rm -fr $tmpdir
