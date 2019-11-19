#!/bin/bash
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Update from https://www.zlib.net/
zlib_version="1.2.11"
zlib_sha256="c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1"
# Update from https://apr.apache.org/
apr_version="1.6.5"
apr_sha256="70dcf9102066a2ff2ffc47e93c289c8e54c95d8dda23b503f9e61bb0cbd2d105"
apr_util_version="1.6.1"
apr_util_sha256="b65e40713da57d004123b6319828be7f1273fbc6490e145874ee1177e112c459"
# Update from https://httpd.apache.org/download.cgi
httpd_version="2.4.38"
httpd_sha256="38d0b73aa313c28065bf58faf64cec12bf7c7d5196146107df2ad07541aa26a6"
# Update from https://www.openssl.org/source/
openssl_version="1.1.1b"
openssl_sha256="5c557b023230413dfb0756f3137a13e6d726838ccd1430888ad15bfb2b43ea4b"
# Update from https://www.pcre.org/
pcre_version="8.42"
pcre_sha256="69acbc2fbdefb955d42a4c606dfde800c2885711d2979e356c0636efde9ec3b5"
# Update from https://secure.php.net/downloads.php
php_version="7.3.3"
php_sha256="9bde40cbf8608ae9c595a6643a02cf0c692c131e2b3619af3fd2af8425d8e677"

build="$PWD/build"
out="$PWD/out"
src="$PWD/src"

if [ -d "$build" ]; then
  echo "$build already exists. Remove for a new build"
  exit 1
fi
if [ -d "$out" ]; then
  echo "$out already exists. Remove for a new build"
  exit 1
fi
if [ -d "$src" ]; then
  echo "$src already exists. Remove for a new build"
  exit 1
fi

jobs=5

echo "Downloading sources"
curl_if_needed() {
  if [ ! -f "$1" ]; then
    curl -o "$1" "$2"
  fi
}
curl_if_needed "apr-${apr_version}.tar.gz" "https://archive.apache.org/dist/apr/apr-${apr_version}.tar.gz"
curl_if_needed "apr-util-${apr_util_version}.tar.gz" "https://archive.apache.org/dist/apr/apr-util-${apr_util_version}.tar.gz"
curl_if_needed "httpd-${httpd_version}.tar.gz" "https://archive.apache.org/dist/httpd/httpd-${httpd_version}.tar.gz"
curl_if_needed "openssl-${openssl_version}.tar.gz" "https://www.openssl.org/source/openssl-${openssl_version}.tar.gz"
curl_if_needed "pcre-${pcre_version}.tar.gz" "https://ftp.pcre.org/pub/pcre/pcre-${pcre_version}.tar.gz"
curl_if_needed "php-${php_version}.tar.gz" "https://secure.php.net/distributions/php-${php_version}.tar.gz"
curl_if_needed "zlib-${zlib_version}.tar.gz" "https://www.zlib.net/zlib-${zlib_version}.tar.gz"

# Check hashes.
cat > SHA256SUMS <<EOT
${apr_sha256}  apr-${apr_version}.tar.gz
${apr_util_sha256}  apr-util-${apr_util_version}.tar.gz
${httpd_sha256}  httpd-${httpd_version}.tar.gz
${openssl_sha256}  openssl-${openssl_version}.tar.gz
${pcre_sha256}  pcre-${pcre_version}.tar.gz
${php_sha256}  php-${php_version}.tar.gz
${zlib_sha256}  zlib-${zlib_version}.tar.gz
EOT
shasum -a 256 -c SHA256SUMS

mkdir "${build}"
mkdir "${src}"

cd "${src}"

export MACOSX_DEPLOYMENT_TARGET="10.10"

echo "Building zlib"
tar xf "../zlib-${zlib_version}.tar.gz"
cd "zlib-${zlib_version}"
./configure --prefix="${build}"
make -j"${jobs}"
make install
cd ..

echo "Building OpenSSL"
tar xf "../openssl-${openssl_version}.tar.gz"
cd "openssl-${openssl_version}"
./config no-tests --prefix="${build}"
make -j"${jobs}"
make install_sw
cd ..

echo "Building PCRE"
tar xf "../pcre-${pcre_version}.tar.gz"
cd "pcre-${pcre_version}"
./configure --prefix="${build}"
make -j"${jobs}"
make install
cd ..

echo "Building APR"
tar xf "../apr-${apr_version}.tar.gz"
cd "apr-${apr_version}"
./configure --prefix="${build}"
make -j"${jobs}"
make install
cd ..

echo "Building APR-util"
tar xf "../apr-util-${apr_util_version}.tar.gz"
cd "apr-util-${apr_util_version}"
./configure --prefix="${build}" --with-apr="${build}"
make -j"${jobs}"
make install
cd ..

echo "Building httpd"
tar xf "../httpd-${httpd_version}.tar.gz"
cd "httpd-${httpd_version}"
# See third_party/blink/tools/apache_config/apache2-httpd-2.4-php7.conf for the
# modules to enable. Build modules as shared libraries to match the LoadModule
# lines (the ServerRoot option will let httpd discover them), but we statically
# link dependencies to avoid runtime linker complications.
./configure --prefix="${build}" \
    --enable-access-compat=shared \
    --enable-actions=shared \
    --enable-alias=shared \
    --enable-asis=shared \
    --enable-authz-core=shared \
    --enable-authz-host=shared \
    --enable-autoindex=shared \
    --enable-cgi=shared \
    --enable-env=shared \
    --enable-headers=shared \
    --enable-imagemap=shared \
    --enable-include=shared \
    --enable-log-config=shared \
    --enable-mime=shared \
    --enable-modules=none \
    --enable-negotiation=shared \
    --enable-rewrite=shared \
    --enable-ssl=shared \
    --enable-unixd=shared \
    --libexecdir="${build}/libexec/apache2" \
    --with-apr-util="${build}" \
    --with-apr="${build}" \
    --with-mpm=prefork \
    --with-pcre="${build}" \
    --with-ssl="${build}"
make -j"${jobs}"
make install
cd ..

echo "Building PHP"
tar xf "../php-${php_version}.tar.gz"
cd "php-${php_version}"
./configure --prefix="${build}" \
    --disable-cgi \
    --disable-cli \
    --with-apxs2="${build}/bin/apxs" \
    --with-zlib="${build}" \
    --without-iconv
make -j"${jobs}"
make install
cd ..

bin_files="
    bin/httpd
    bin/openssl"
lib_files="
    lib/libapr-1.0.dylib
    lib/libaprutil-1.0.dylib
    lib/libcrypto.1.1.dylib
    lib/libpcre.1.dylib
    lib/libpcrecpp.0.dylib
    lib/libpcreposix.0.dylib
    lib/libssl.1.1.dylib
    lib/libz.1.2.11.dylib"
libexec_files="
    libexec/apache2/libphp7.so
    libexec/apache2/mod_access_compat.so
    libexec/apache2/mod_actions.so
    libexec/apache2/mod_alias.so
    libexec/apache2/mod_asis.so
    libexec/apache2/mod_authz_core.so
    libexec/apache2/mod_authz_host.so
    libexec/apache2/mod_autoindex.so
    libexec/apache2/mod_cgi.so
    libexec/apache2/mod_env.so
    libexec/apache2/mod_headers.so
    libexec/apache2/mod_imagemap.so
    libexec/apache2/mod_include.so
    libexec/apache2/mod_log_config.so
    libexec/apache2/mod_mime.so
    libexec/apache2/mod_negotiation.so
    libexec/apache2/mod_rewrite.so
    libexec/apache2/mod_ssl.so
    libexec/apache2/mod_unixd.so"
license_files="
    apr-${apr_version}/LICENSE
    apr-${apr_version}/NOTICE
    apr-util-${apr_util_version}/LICENSE
    apr-util-${apr_util_version}/NOTICE
    httpd-${httpd_version}/LICENSE
    httpd-${httpd_version}/NOTICE
    openssl-${openssl_version}/LICENSE
    pcre-${pcre_version}/LICENCE
    php-${php_version}/LICENSE"

echo "Copying files"
mkdir "${out}"
mkdir "${out}/bin"
mkdir "${out}/lib"
mkdir "${out}/libexec"
mkdir "${out}/libexec/apache2"

cat > "${out}/LICENSE" <<EOT
This directory contains binaries for Apache httpd, PHP, and their dependencies.
License and notices for each are listed below:
EOT

for f in ${license_files}; do
  echo >> "${out}/LICENSE"
  echo "=======================" >> "${out}/LICENSE"
  echo >> "${out}/LICENSE"
  echo "${f}:" >> "${out}/LICENSE"
  cat "${src}/${f}" >> "${out}/LICENSE"
done

# zlib does not have a standalone LICENSE file. Extract it from the README
# instead.
echo >> "${out}/LICENSE"
echo "=======================" >> "${out}/LICENSE"
echo >> "${out}/LICENSE"
echo "From zlib-${zlib_version}/README:" >> "${out}/LICENSE"
sed -n -e '/^Copyright notice:/,//p' "${src}/zlib-${zlib_version}/README" >> "${out}/LICENSE"

for f in ${bin_files} ${lib_files} ${libexec_files}; do
  cp "${build}/${f}" "${out}/${f}"
  for lib in ${lib_files}; do
    install_name_tool -change "${build}/${lib}" "@rpath/$(basename "${lib}")" "${out}/${f}"
  done
done
for f in ${bin_files}; do
  install_name_tool -add_rpath "@executable_path/../lib" "${out}/${f}"
done
for f in ${lib_files}; do
  install_name_tool -id "@rpath/$(basename "${f}")" "${out}/${f}"
done
for f in ${libexec_files}; do
  install_name_tool -id "@rpath/../libexec/$(basename "${f}")" "${out}/${f}"
done
