/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_x509_certificate_private.idl,
 *   modified Wed Apr 11 17:11:26 2012.
 */

#ifndef PPAPI_C_PRIVATE_PPB_X509_CERTIFICATE_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_X509_CERTIFICATE_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1 \
    "PPB_X509Certificate_Private;0.1"
#define PPB_X509CERTIFICATE_PRIVATE_INTERFACE \
    PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_X509Certificate_Private</code> interface for
 * an X509 certificate.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration corresponds to fields of an X509 certificate. Refer to
 * <a href="http://www.ietf.org/rfc/rfc5280.txt>RFC 5280</a> for further
 * documentation about particular fields.
 */
typedef enum {
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_COMMON_NAME = 0,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_LOCALITY_NAME = 1,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_STATE_OR_PROVINCE_NAME = 2,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_COUNTRY_NAME = 3,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_NAME = 4,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_UNIT_NAME = 5,
  /**
   * Note: This field is unimplemented and will return
   * <code>PP_VARTYPE_NULL</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_UNIQUE_ID = 6,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_COMMON_NAME = 7,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_LOCALITY_NAME = 8,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_STATE_OR_PROVINCE_NAME = 9,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_COUNTRY_NAME = 10,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_NAME = 11,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_UNIT_NAME = 12,
  /**
   * Note: This field is unimplemented and will return
   * <code>PP_VARTYPE_NULL</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_UNIQUE_ID = 13,
  /**
   * Note: This field is unimplemented and will return
   * <code>PP_VARTYPE_NULL</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_VERSION = 14,
  /**
   * This corresponds to a byte array (<code>PP_VARTYPE_ARRAY_BUFFER</code>).
   * The serial number may include a leading 0.
   */
  PP_X509CERTIFICATE_PRIVATE_SERIAL_NUMBER = 15,
  /**
   * Note: This field is unimplemented and will return
   * <code>PP_VARTYPE_NULL</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_SIGNATURE_ALGORITHM_OID = 16,
  /**
   * Note: This field is unimplemented and will return
   * <code>PP_VARTYPE_NULL</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_SIGNATURE_ALGORITHM_PARAMATERS_RAW = 17,
  /**
   * This corresponds to a double (<code>PP_VARTYPE_DOUBLE</code>) which
   * can be cast to a <code>PP_TIME</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_BEFORE = 18,
  /**
   * This corresponds to a double (<code>PP_VARTYPE_DOUBLE</code>) which
   * can be cast to a <code>PP_TIME</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_AFTER = 19,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_PUBLIC_KEY_ALGORITHM_OID = 20,
  /**
   * Note: This field is unimplemented and will return
   * <code>PP_VARTYPE_NULL</code>.
   */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_PUBLIC_KEY = 21,
  /**
   * This corresponds to a byte array (<code>PP_VARTYPE_ARRAY_BUFFER</code>).
   * This is the DER-encoded representation of the certificate.
   */
  PP_X509CERTIFICATE_PRIVATE_RAW = 22,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_ISSUER_DISTINGUISHED_NAME = 23,
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_X509CERTIFICATE_PRIVATE_SUBJECT_DISTINGUISHED_NAME = 24
} PP_X509Certificate_Private_Field;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_X509Certificate_Private_Field, 4);

/**
 * This enumeration defines the different possible values for X5O9 certificate
 * versions as returned by:
 * <code>GetField(resource, PP_X509CERTIFICATE_PRIVATE_VERSION)</code>.
 */
typedef enum {
  PP_X509CERTIFICATE_PRIVATE_V1 = 0,
  PP_X509CERTIFICATE_PRIVATE_V2 = 1,
  PP_X509CERTIFICATE_PRIVATE_V3 = 2
} PPB_X509Certificate_Private_Version;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PPB_X509Certificate_Private_Version, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_X509Certificate_Private</code> interface provides access to
 * the fields of an X509 certificate.
 */
struct PPB_X509Certificate_Private_0_1 {
  /**
   * Allocates a <code>PPB_X509Certificate_Private</code> resource.
   * <code>Initialize()</code> must be called before using the certificate.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Returns <code>PP_TRUE</code> if a given resource is a
   * <code>PPB_X509Certificate_Private</code>.
   */
  PP_Bool (*IsX509CertificatePrivate)(PP_Resource resource);
  /**
   * Initializes a <code>PPB_X509Certificate_Private</code> from the DER-encoded
   * representation. |bytes| should represent only a single certificate.
   * <code>PP_FALSE</code> is returned if |bytes| is not a valid DER-encoding of
   * a certificate. Note: Flash requires this to be synchronous.
   */
  PP_Bool (*Initialize)(PP_Resource resource,
                        const char* bytes,
                        uint32_t length);
  /**
   * Get a field of the X509Certificate as a <code>PP_Var</code>. A null
   * <code>PP_Var</code> is returned if the field is unavailable.
   */
  struct PP_Var (*GetField)(PP_Resource resource,
                            PP_X509Certificate_Private_Field field);
};

typedef struct PPB_X509Certificate_Private_0_1 PPB_X509Certificate_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_X509_CERTIFICATE_PRIVATE_H_ */

