// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_certificate_chain.h"

#include <algorithm>

#include "base/check.h"
#include "net/cert/internal/cert_error_params.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/internal/extended_key_usage.h"
#include "net/cert/internal/name_constraints.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/trust_store.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/der/input.h"
#include "net/der/parser.h"

namespace net {

namespace {

bool IsHandledCriticalExtension(const ParsedExtension& extension) {
  if (extension.oid == BasicConstraintsOid())
    return true;
  // Key Usage is NOT processed for end-entity certificates (this is the
  // responsibility of callers), however it is considered "handled" here in
  // order to allow being marked as critical.
  if (extension.oid == KeyUsageOid())
    return true;
  if (extension.oid == ExtKeyUsageOid())
    return true;
  if (extension.oid == NameConstraintsOid())
    return true;
  if (extension.oid == SubjectAltNameOid())
    return true;
  if (extension.oid == CertificatePoliciesOid()) {
    // Policy qualifiers are skipped during processing, so if the
    // extension is marked critical need to ensure there weren't any
    // qualifiers other than User Notice / CPS.
    //
    // This follows from RFC 5280 section 4.2.1.4:
    //
    //   If this extension is critical, the path validation software MUST
    //   be able to interpret this extension (including the optional
    //   qualifier), or MUST reject the certificate.
    std::vector<der::Input> unused_policies;
    CertErrors unused_errors;
    return ParseCertificatePoliciesExtension(
        extension.value, true /*fail_parsing_unknown_qualifier_oids*/,
        &unused_policies, &unused_errors);

    // TODO(eroman): Give a better error message.
  }
  if (extension.oid == PolicyMappingsOid())
    return true;
  if (extension.oid == PolicyConstraintsOid())
    return true;
  if (extension.oid == InhibitAnyPolicyOid())
    return true;

  return false;
}

// Adds errors to |errors| if the certificate contains unconsumed _critical_
// extensions.
void VerifyNoUnconsumedCriticalExtensions(const ParsedCertificate& cert,
                                          CertErrors* errors) {
  for (const auto& it : cert.extensions()) {
    const ParsedExtension& extension = it.second;
    if (extension.critical && !IsHandledCriticalExtension(extension)) {
      errors->AddError(cert_errors::kUnconsumedCriticalExtension,
                       CreateCertErrorParams2Der("oid", extension.oid, "value",
                                                 extension.value));
    }
  }
}

// Returns true if |cert| was self-issued. The definition of self-issuance
// comes from RFC 5280 section 6.1:
//
//    A certificate is self-issued if the same DN appears in the subject
//    and issuer fields (the two DNs are the same if they match according
//    to the rules specified in Section 7.1).  In general, the issuer and
//    subject of the certificates that make up a path are different for
//    each certificate.  However, a CA may issue a certificate to itself to
//    support key rollover or changes in certificate policies.  These
//    self-issued certificates are not counted when evaluating path length
//    or name constraints.
WARN_UNUSED_RESULT bool IsSelfIssued(const ParsedCertificate& cert) {
  return cert.normalized_subject() == cert.normalized_issuer();
}

// Adds errors to |errors| if |cert| is not valid at time |time|.
//
// The certificate's validity requirements are described by RFC 5280 section
// 4.1.2.5:
//
//    The validity period for a certificate is the period of time from
//    notBefore through notAfter, inclusive.
void VerifyTimeValidity(const ParsedCertificate& cert,
                        const der::GeneralizedTime& time,
                        CertErrors* errors) {
  if (time < cert.tbs().validity_not_before)
    errors->AddError(cert_errors::kValidityFailedNotBefore);

  if (cert.tbs().validity_not_after < time)
    errors->AddError(cert_errors::kValidityFailedNotAfter);
}

// Adds errors to |errors| if |cert| has internally inconsistent signature
// algorithms.
//
// X.509 certificates contain two different signature algorithms:
//  (1) The signatureAlgorithm field of Certificate
//  (2) The signature field of TBSCertificate
//
// According to RFC 5280 section 4.1.1.2 and 4.1.2.3 these two fields must be
// equal:
//
//     This field MUST contain the same algorithm identifier as the
//     signature field in the sequence tbsCertificate (Section 4.1.2.3).
//
// The spec is not explicit about what "the same algorithm identifier" means.
// Our interpretation is that the two DER-encoded fields must be byte-for-byte
// identical.
//
// In practice however there are certificates which use different encodings for
// specifying RSA with SHA1 (different OIDs). This is special-cased for
// compatibility sake.
bool VerifySignatureAlgorithmsMatch(const ParsedCertificate& cert,
                                    CertErrors* errors) {
  const der::Input& alg1_tlv = cert.signature_algorithm_tlv();
  const der::Input& alg2_tlv = cert.tbs().signature_algorithm_tlv;

  // Ensure that the two DER-encoded signature algorithms are byte-for-byte
  // equal.
  if (alg1_tlv == alg2_tlv)
    return true;

  // But make a compatibility concession if alternate encodings are used
  // TODO(eroman): Turn this warning into an error.
  // TODO(eroman): Add a unit-test that exercises this case.
  if (SignatureAlgorithm::IsEquivalent(alg1_tlv, alg2_tlv)) {
    errors->AddWarning(
        cert_errors::kSignatureAlgorithmsDifferentEncoding,
        CreateCertErrorParams2Der("Certificate.algorithm", alg1_tlv,
                                  "TBSCertificate.signature", alg2_tlv));
    return true;
  }

  errors->AddError(
      cert_errors::kSignatureAlgorithmMismatch,
      CreateCertErrorParams2Der("Certificate.algorithm", alg1_tlv,
                                "TBSCertificate.signature", alg2_tlv));
  return false;
}

// Verify that |cert| can be used for |required_key_purpose|.
void VerifyExtendedKeyUsage(const ParsedCertificate& cert,
                            KeyPurpose required_key_purpose,
                            CertErrors* errors) {
  switch (required_key_purpose) {
    case KeyPurpose::ANY_EKU:
      return;
    case KeyPurpose::SERVER_AUTH: {
      // TODO(eroman): Is it OK for the target certificate to omit the EKU?
      if (!cert.has_extended_key_usage())
        return;

      for (const auto& key_purpose_oid : cert.extended_key_usage()) {
        if (key_purpose_oid == AnyEKU())
          return;
        if (key_purpose_oid == ServerAuth())
          return;
      }

      // Check if the certificate contains Netscape Server Gated Crypto.
      // nsSGC is a deprecated mechanism, and not part of RFC 5280's
      // profile. Some unexpired certificate chains still rely on it though
      // (there are intermediates valid until 2020 that use it).
      bool has_nsgc = false;

      for (const auto& key_purpose_oid : cert.extended_key_usage()) {
        if (key_purpose_oid == NetscapeServerGatedCrypto()) {
          has_nsgc = true;
          break;
        }
      }

      if (has_nsgc) {
        errors->AddWarning(cert_errors::kEkuLacksServerAuthButHasGatedCrypto);

        // Allow NSGC for legacy RSA SHA1 intermediates, for compatibility with
        // platform verifiers.
        //
        // In practice the chain will be rejected with or without this
        // compatibility hack. The difference is whether the final error will be
        // ERR_CERT_WEAK_SIGNATURE_ALGORITHM  (with compatibility hack) vs
        // ERR_CERT_INVALID (without hack).
        //
        // TODO(https://crbug.com/843735): Remove this once error-for-error
        // equivalence between builtin verifier and platform verifier is less
        // important.
        if ((cert.has_basic_constraints() && cert.basic_constraints().is_ca) &&
            (cert.signature_algorithm().algorithm() ==
             SignatureAlgorithmId::RsaPkcs1) &&
            (cert.signature_algorithm().digest() == DigestAlgorithm::Sha1)) {
          return;
        }
      }

      errors->AddError(cert_errors::kEkuLacksServerAuth);
      break;
    }
    case KeyPurpose::CLIENT_AUTH: {
      // TODO(eroman): Is it OK for the target certificate to omit the EKU?
      if (!cert.has_extended_key_usage())
        return;

      for (const auto& key_purpose_oid : cert.extended_key_usage()) {
        if (key_purpose_oid == AnyEKU())
          return;
        if (key_purpose_oid == ClientAuth())
          return;
      }

      errors->AddError(cert_errors::kEkuLacksClientAuth);
      break;
    }
  }
}

// Returns |true| if |policies| contains the OID |search_oid|.
bool SetContains(const std::set<der::Input>& policies,
                 const der::Input& search_oid) {
  return policies.count(search_oid) > 0;
}

// Representation of RFC 5280's "valid_policy_tree", used to keep track of the
// valid policies and policy re-mappings.
//
// ValidPolicyTree differs slightly from RFC 5280's description in that:
//
//  (1) It does not track "qualifier_set". This is not needed as it is not
//      output by this implementation.
//
//  (2) It only stores the most recent level of the policy tree rather than
//      the full tree of nodes.
class ValidPolicyTree {
 public:
  ValidPolicyTree() = default;

  struct Node {
    // |root_policy| is equivalent to |valid_policy|, but in the domain of the
    // caller.
    //
    // The reason for this distinction is the Policy Mappings extension.
    //
    // So whereas |valid_policy| is in the remapped domain defined by the
    // issuing certificate, |root_policy| is in the fixed domain of the caller.
    //
    // OIDs in "user_initial_policy_set" and "user_constrained_policy_set" are
    // directly comparable to |root_policy| values, but not necessarily to
    // |valid_policy|.
    //
    // In terms of the valid policy tree, |root_policy| can be found by
    // starting at the node's root ancestor, and finding the first node with a
    // valid_policy other than anyPolicy. This is effectively the same process
    // as used during policy tree intersection in RFC 5280 6.1.5.g.iii.1
    der::Input root_policy;

    // The same as RFC 5280's "valid_policy" variable.
    der::Input valid_policy;

    // The same as RFC 5280s "expected_policy_set" variable.
    std::set<der::Input> expected_policy_set;

    // Note that RFC 5280's "qualifier_set" is omitted.
  };

  // Level represents all the nodes at depth "i" in the valid_policy_tree.
  using Level = std::vector<Node>;

  // Initializes the ValidPolicyTree for the given "user_initial_policy_set".
  //
  // In RFC 5280, the valid_policy_tree is initialized to a root node at depth
  // 0 of "anyPolicy"; the intersection with the "user_initial_policy_set" is
  // done at the end (Wrap Up) as described in section 6.1.5 step g.
  //
  // Whereas in this implementation, the restriction on policies is added here,
  // and intersecting the valid policy tree during Wrap Up is no longer needed.
  //
  // The final "user_constrained_policy_set" obtained will be the same. The
  // advantages of this approach is simpler code.
  void Init(const std::set<der::Input>& user_initial_policy_set) {
    Clear();
    for (const der::Input& policy_oid : user_initial_policy_set)
      AddRootNode(policy_oid);
  }

  // Returns the current level (i.e. all nodes at depth i in the valid
  // policy tree).
  const Level& current_level() const { return current_level_; }
  Level& current_level() { return current_level_; }

  // In RFC 5280 valid_policy_tree may be set to null. That is represented here
  // by emptiness.
  bool IsNull() const { return current_level_.empty(); }
  void SetNull() { Clear(); }

  // This implementation keeps only the last level of the valid policy
  // tree. Calling StartLevel() returns the nodes for the previous
  // level, and starts a new level.
  Level StartLevel() {
    Level prev_level;
    std::swap(prev_level, current_level_);
    return prev_level;
  }

  // Gets the set of policies (in terms of root authority's policy domain) that
  // are valid at the curent level of the policy tree.
  //
  // For example:
  //
  //  * If the valid policy tree was initialized with anyPolicy, then this
  //    function returns what X.509 calls "authorities-constrained-policy-set".
  //
  //  * If the valid policy tree was instead initialized with the
  //    "user-initial-policy_set", then this function returns what X.509
  //    calls "user-constrained-policy-set"
  //    ("authorities-constrained-policy-set" intersected with the
  //    "user-initial-policy-set").
  void GetValidRootPolicySet(std::set<der::Input>* policy_set) {
    policy_set->clear();
    for (const Node& node : current_level_)
      policy_set->insert(node.root_policy);

    // If the result includes anyPolicy, simplify it to a set of size 1.
    if (policy_set->size() > 1 && SetContains(*policy_set, AnyPolicy()))
      *policy_set = {AnyPolicy()};
  }

  // Adds a node |n| to the current level which is a child of |parent|
  // such that:
  //   * n.valid_policy = policy_oid
  //   * n.expected_policy_set = {policy_oid}
  void AddNode(const Node& parent, const der::Input& policy_oid) {
    AddNodeWithExpectedPolicySet(parent, policy_oid, {policy_oid});
  }

  // Adds a node |n| to the current level which is a child of |parent|
  // such that:
  //   * n.valid_policy = policy_oid
  //   * n.expected_policy_set = expected_policy_set
  void AddNodeWithExpectedPolicySet(
      const Node& parent,
      const der::Input& policy_oid,
      const std::set<der::Input>& expected_policy_set) {
    Node new_node;
    new_node.valid_policy = policy_oid;
    new_node.expected_policy_set = expected_policy_set;

    // Consider the root policy as the first policy other than anyPolicy (or
    // anyPolicy if it hasn't been restricted yet).
    new_node.root_policy =
        (parent.root_policy == AnyPolicy()) ? policy_oid : parent.root_policy;

    current_level_.push_back(std::move(new_node));
  }

  // Returns the first node having valid_policy == anyPolicy in |level|, or
  // nullptr if there is none.
  static const Node* FindAnyPolicyNode(const Level& level) {
    for (const Node& node : level) {
      if (node.valid_policy == AnyPolicy())
        return &node;
    }
    return nullptr;
  }

  // Deletes all nodes |n| in |level| where |n.valid_policy| matches the given
  // |valid_policy|. This may re-order the nodes in |level|.
  static void DeleteNodesMatchingValidPolicy(const der::Input& valid_policy,
                                             Level* level) {
    // This works by swapping nodes to the end of the vector, and then doing a
    // single resize to delete them all.
    auto cur = level->begin();
    auto end = level->end();
    while (cur != end) {
      bool should_delete_node = cur->valid_policy == valid_policy;
      if (should_delete_node) {
        end = std::prev(end);
        if (cur != end)
          std::iter_swap(cur, end);
      } else {
        ++cur;
      }
    }
    level->erase(end, level->end());
  }

 private:
  // Deletes all nodes in the valid policy tree.
  void Clear() { current_level_.clear(); }

  // Adds a node to the current level for OID |policy_oid|. The current level
  // is assumed to be the root level.
  void AddRootNode(const der::Input& policy_oid) {
    Node new_node;
    new_node.root_policy = policy_oid;
    new_node.valid_policy = policy_oid;
    new_node.expected_policy_set = {policy_oid};
    current_level_.push_back(std::move(new_node));
  }

  Level current_level_;

  DISALLOW_COPY_AND_ASSIGN(ValidPolicyTree);
};

// Class that encapsulates the state variables used by certificate path
// validation.
class PathVerifier {
 public:
  // Same parameters and meaning as VerifyCertificateChain().
  void Run(const ParsedCertificateList& certs,
           const CertificateTrust& last_cert_trust,
           VerifyCertificateChainDelegate* delegate,
           const der::GeneralizedTime& time,
           KeyPurpose required_key_purpose,
           InitialExplicitPolicy initial_explicit_policy,
           const std::set<der::Input>& user_initial_policy_set,
           InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
           InitialAnyPolicyInhibit initial_any_policy_inhibit,
           std::set<der::Input>* user_constrained_policy_set,
           CertPathErrors* errors);

 private:
  // Verifies and updates the valid policies. This corresponds with RFC 5280
  // section 6.1.3 steps d-f.
  void VerifyPolicies(const ParsedCertificate& cert,
                      bool is_target_cert,
                      CertErrors* errors);

  // Applies the policy mappings. This corresponds with RFC 5280 section 6.1.4
  // steps a-b.
  void VerifyPolicyMappings(const ParsedCertificate& cert, CertErrors* errors);

  // This function corresponds to RFC 5280 section 6.1.3's "Basic Certificate
  // Processing" procedure.
  void BasicCertificateProcessing(const ParsedCertificate& cert,
                                  bool is_target_cert,
                                  const der::GeneralizedTime& time,
                                  KeyPurpose required_key_purpose,
                                  CertErrors* errors,
                                  bool* shortcircuit_chain_validation);

  // This function corresponds to RFC 5280 section 6.1.4's "Preparation for
  // Certificate i+1" procedure. |cert| is expected to be an intermediate.
  void PrepareForNextCertificate(const ParsedCertificate& cert,
                                 CertErrors* errors);

  // This function corresponds with RFC 5280 section 6.1.5's "Wrap-Up
  // Procedure". It does processing for the final certificate (the target cert).
  void WrapUp(const ParsedCertificate& cert, CertErrors* errors);

  // Enforces trust anchor constraints compatibile with RFC 5937.
  //
  // Note that the anchor constraints are encoded via the attached certificate
  // itself.
  void ApplyTrustAnchorConstraints(const ParsedCertificate& cert,
                                   KeyPurpose required_key_purpose,
                                   CertErrors* errors);

  // Initializes the path validation algorithm given anchor constraints. This
  // follows the description in RFC 5937
  void ProcessRootCertificate(const ParsedCertificate& cert,
                              const CertificateTrust& trust,
                              KeyPurpose required_key_purpose,
                              CertErrors* errors,
                              bool* shortcircuit_chain_validation);

  // Parses |spki| to an EVP_PKEY and checks whether the public key is accepted
  // by |delegate_|. On failure parsing returns nullptr. If either parsing the
  // key or key policy failed, adds a high-severity error to |errors|.
  bssl::UniquePtr<EVP_PKEY> ParseAndCheckPublicKey(const der::Input& spki,
                                                   CertErrors* errors);

  ValidPolicyTree valid_policy_tree_;

  // Will contain a NameConstraints for each previous cert in the chain which
  // had nameConstraints. This corresponds to the permitted_subtrees and
  // excluded_subtrees state variables from RFC 5280.
  std::vector<const NameConstraints*> name_constraints_list_;

  // |explicit_policy_| corresponds with the same named variable from RFC 5280
  // section 6.1.2:
  //
  //   explicit_policy:  an integer that indicates if a non-NULL
  //   valid_policy_tree is required.  The integer indicates the
  //   number of non-self-issued certificates to be processed before
  //   this requirement is imposed.  Once set, this variable may be
  //   decreased, but may not be increased.  That is, if a certificate in the
  //   path requires a non-NULL valid_policy_tree, a later certificate cannot
  //   remove this requirement.  If initial-explicit-policy is set, then the
  //   initial value is 0, otherwise the initial value is n+1.
  size_t explicit_policy_;

  // |inhibit_any_policy_| corresponds with the same named variable from RFC
  // 5280 section 6.1.2:
  //
  //   inhibit_anyPolicy:  an integer that indicates whether the
  //   anyPolicy policy identifier is considered a match.  The
  //   integer indicates the number of non-self-issued certificates
  //   to be processed before the anyPolicy OID, if asserted in a
  //   certificate other than an intermediate self-issued
  //   certificate, is ignored.  Once set, this variable may be
  //   decreased, but may not be increased.  That is, if a
  //   certificate in the path inhibits processing of anyPolicy, a
  //   later certificate cannot permit it.  If initial-any-policy-
  //   inhibit is set, then the initial value is 0, otherwise the
  //   initial value is n+1.
  size_t inhibit_any_policy_;

  // |policy_mapping_| corresponds with the same named variable from RFC 5280
  // section 6.1.2:
  //
  //   policy_mapping:  an integer that indicates if policy mapping
  //   is permitted.  The integer indicates the number of non-self-
  //   issued certificates to be processed before policy mapping is
  //   inhibited.  Once set, this variable may be decreased, but may
  //   not be increased.  That is, if a certificate in the path
  //   specifies that policy mapping is not permitted, it cannot be
  //   overridden by a later certificate.  If initial-policy-
  //   mapping-inhibit is set, then the initial value is 0,
  //   otherwise the initial value is n+1.
  size_t policy_mapping_;

  // |working_public_key_| is an amalgamation of 3 separate variables from RFC
  // 5280:
  //    * working_public_key
  //    * working_public_key_algorithm
  //    * working_public_key_parameters
  //
  // They are combined for simplicity since the signature verification takes an
  // EVP_PKEY, and the parameter inheritence is not applicable for the supported
  // key types. |working_public_key_| may be null if parsing failed.
  //
  // An approximate explanation of |working_public_key_| is this description
  // from RFC 5280 section 6.1.2:
  //
  //    working_public_key:  the public key used to verify the
  //    signature of a certificate.
  bssl::UniquePtr<EVP_PKEY> working_public_key_;

  // |working_normalized_issuer_name_| is the normalized value of the
  // working_issuer_name variable in RFC 5280 section 6.1.2:
  //
  //    working_issuer_name:  the issuer distinguished name expected
  //    in the next certificate in the chain.
  der::Input working_normalized_issuer_name_;

  // |max_path_length_| corresponds with the same named variable in RFC 5280
  // section 6.1.2.
  //
  //    max_path_length:  this integer is initialized to n, is
  //    decremented for each non-self-issued certificate in the path,
  //    and may be reduced to the value in the path length constraint
  //    field within the basic constraints extension of a CA
  //    certificate.
  size_t max_path_length_;

  VerifyCertificateChainDelegate* delegate_;
};

void PathVerifier::VerifyPolicies(const ParsedCertificate& cert,
                                  bool is_target_cert,
                                  CertErrors* errors) {
  // From RFC 5280 section 6.1.3:
  //
  //  (d)  If the certificate policies extension is present in the
  //       certificate and the valid_policy_tree is not NULL, process
  //       the policy information by performing the following steps in
  //       order:
  if (cert.has_policy_oids() && !valid_policy_tree_.IsNull()) {
    ValidPolicyTree::Level previous_level = valid_policy_tree_.StartLevel();

    // Identify if there was a node with valid_policy == anyPolicy at depth i-1.
    const ValidPolicyTree::Node* any_policy_node_prev_level =
        ValidPolicyTree::FindAnyPolicyNode(previous_level);

    //     (1)  For each policy P not equal to anyPolicy in the
    //          certificate policies extension, let P-OID denote the OID
    //          for policy P and P-Q denote the qualifier set for policy
    //          P.  Perform the following steps in order:
    bool cert_has_any_policy = false;
    for (const der::Input& p_oid : cert.policy_oids()) {
      if (p_oid == AnyPolicy()) {
        cert_has_any_policy = true;
        continue;
      }

      //        (i)   For each node of depth i-1 in the valid_policy_tree
      //              where P-OID is in the expected_policy_set, create a
      //              child node as follows: set the valid_policy to P-OID,
      //              set the qualifier_set to P-Q, and set the
      //              expected_policy_set to {P-OID}.
      bool found_match = false;
      for (const ValidPolicyTree::Node& prev_node : previous_level) {
        if (SetContains(prev_node.expected_policy_set, p_oid)) {
          valid_policy_tree_.AddNode(prev_node, p_oid);
          found_match = true;
        }
      }

      //        (ii)  If there was no match in step (i) and the
      //              valid_policy_tree includes a node of depth i-1 with
      //              the valid_policy anyPolicy, generate a child node with
      //              the following values: set the valid_policy to P-OID,
      //              set the qualifier_set to P-Q, and set the
      //              expected_policy_set to  {P-OID}.
      if (!found_match && any_policy_node_prev_level)
        valid_policy_tree_.AddNode(*any_policy_node_prev_level, p_oid);
    }

    //     (2)  If the certificate policies extension includes the policy
    //          anyPolicy with the qualifier set AP-Q and either (a)
    //          inhibit_anyPolicy is greater than 0 or (b) i<n and the
    //          certificate is self-issued, then:
    //
    //          For each node in the valid_policy_tree of depth i-1, for
    //          each value in the expected_policy_set (including
    //          anyPolicy) that does not appear in a child node, create a
    //          child node with the following values: set the valid_policy
    //          to the value from the expected_policy_set in the parent
    //          node, set the qualifier_set to AP-Q, and set the
    //          expected_policy_set to the value in the valid_policy from
    //          this node.
    if (cert_has_any_policy && ((inhibit_any_policy_ > 0) ||
                                (!is_target_cert && IsSelfIssued(cert)))) {
      // Keep track of the existing policies at depth i.
      std::set<der::Input> child_node_policies;
      for (const ValidPolicyTree::Node& node :
           valid_policy_tree_.current_level())
        child_node_policies.insert(node.valid_policy);

      for (const ValidPolicyTree::Node& prev_node : previous_level) {
        for (const der::Input& expected_policy :
             prev_node.expected_policy_set) {
          if (!SetContains(child_node_policies, expected_policy)) {
            child_node_policies.insert(expected_policy);
            valid_policy_tree_.AddNode(prev_node, expected_policy);
          }
        }
      }
    }

    //     (3)  If there is a node in the valid_policy_tree of depth i-1
    //          or less without any child nodes, delete that node.  Repeat
    //          this step until there are no nodes of depth i-1 or less
    //          without children.
    //
    // Nothing needs to be done for this step, since this implementation only
    // stores the nodes at depth i, and the entire level has already been
    // calculated.
  }

  //  (e)  If the certificate policies extension is not present, set the
  //       valid_policy_tree to NULL.
  if (!cert.has_policy_oids())
    valid_policy_tree_.SetNull();

  //  (f)  Verify that either explicit_policy is greater than 0 or the
  //       valid_policy_tree is not equal to NULL;
  if (!((explicit_policy_ > 0) || !valid_policy_tree_.IsNull()))
    errors->AddError(cert_errors::kNoValidPolicy);
}

void PathVerifier::VerifyPolicyMappings(const ParsedCertificate& cert,
                                        CertErrors* errors) {
  if (!cert.has_policy_mappings())
    return;

  // From RFC 5280 section 6.1.4:
  //
  //  (a)  If a policy mappings extension is present, verify that the
  //       special value anyPolicy does not appear as an
  //       issuerDomainPolicy or a subjectDomainPolicy.
  for (const ParsedPolicyMapping& mapping : cert.policy_mappings()) {
    if (mapping.issuer_domain_policy == AnyPolicy() ||
        mapping.subject_domain_policy == AnyPolicy()) {
      // Because this implementation continues processing certificates after
      // this error, clear the valid policy tree to ensure the
      // "user_constrained_policy_set" output upon failure is empty.
      valid_policy_tree_.SetNull();
      errors->AddError(cert_errors::kPolicyMappingAnyPolicy);
    }
  }

  //  (b)  If a policy mappings extension is present, then for each
  //       issuerDomainPolicy ID-P in the policy mappings extension:
  //
  //     (1)  If the policy_mapping variable is greater than 0, for each
  //          node in the valid_policy_tree of depth i where ID-P is the
  //          valid_policy, set expected_policy_set to the set of
  //          subjectDomainPolicy values that are specified as
  //          equivalent to ID-P by the policy mappings extension.
  //
  //          If no node of depth i in the valid_policy_tree has a
  //          valid_policy of ID-P but there is a node of depth i with a
  //          valid_policy of anyPolicy, then generate a child node of
  //          the node of depth i-1 that has a valid_policy of anyPolicy
  //          as follows:
  //
  //        (i)    set the valid_policy to ID-P;
  //
  //        (ii)   set the qualifier_set to the qualifier set of the
  //               policy anyPolicy in the certificate policies
  //               extension of certificate i; and
  //
  //        (iii)  set the expected_policy_set to the set of
  //               subjectDomainPolicy values that are specified as
  //               equivalent to ID-P by the policy mappings extension.
  //
  if (policy_mapping_ > 0) {
    const ValidPolicyTree::Node* any_policy_node =
        ValidPolicyTree::FindAnyPolicyNode(valid_policy_tree_.current_level());

    // Group mappings by issuer domain policy.
    std::map<der::Input, std::set<der::Input>> mappings;
    for (const ParsedPolicyMapping& mapping : cert.policy_mappings()) {
      mappings[mapping.issuer_domain_policy].insert(
          mapping.subject_domain_policy);
    }

    for (const auto& it : mappings) {
      const der::Input& issuer_domain_policy = it.first;
      const std::set<der::Input>& subject_domain_policies = it.second;
      bool found_node = false;

      for (ValidPolicyTree::Node& node : valid_policy_tree_.current_level()) {
        if (node.valid_policy == issuer_domain_policy) {
          node.expected_policy_set = subject_domain_policies;
          found_node = true;
        }
      }

      if (!found_node && any_policy_node) {
        valid_policy_tree_.AddNodeWithExpectedPolicySet(
            *any_policy_node, issuer_domain_policy, subject_domain_policies);
      }
    }
  }

  //  (b)  If a policy mappings extension is present, then for each
  //       issuerDomainPolicy ID-P in the policy mappings extension:
  //
  //  ...
  //
  //     (2)  If the policy_mapping variable is equal to 0:
  //
  //        (i)    delete each node of depth i in the valid_policy_tree
  //               where ID-P is the valid_policy.
  //
  //        (ii)   If there is a node in the valid_policy_tree of depth
  //               i-1 or less without any child nodes, delete that
  //               node.  Repeat this step until there are no nodes of
  //               depth i-1 or less without children.
  if (policy_mapping_ == 0) {
    for (const ParsedPolicyMapping& mapping : cert.policy_mappings()) {
      ValidPolicyTree::DeleteNodesMatchingValidPolicy(
          mapping.issuer_domain_policy, &valid_policy_tree_.current_level());
    }
  }
}

void PathVerifier::BasicCertificateProcessing(
    const ParsedCertificate& cert,
    bool is_target_cert,
    const der::GeneralizedTime& time,
    KeyPurpose required_key_purpose,
    CertErrors* errors,
    bool* shortcircuit_chain_validation) {
  *shortcircuit_chain_validation = false;
  // Check that the signature algorithms in Certificate vs TBSCertificate
  // match. This isn't part of RFC 5280 section 6.1.3, but is mandated by
  // sections 4.1.1.2 and 4.1.2.3.
  if (!VerifySignatureAlgorithmsMatch(cert, errors))
    *shortcircuit_chain_validation = true;

  // Check whether this signature algorithm is allowed.
  if (!delegate_->IsSignatureAlgorithmAcceptable(cert.signature_algorithm(),
                                                 errors)) {
    *shortcircuit_chain_validation = true;
    errors->AddError(cert_errors::kUnacceptableSignatureAlgorithm);
  }

  if (working_public_key_) {
    // Verify the digital signature using the previous certificate's key (RFC
    // 5280 section 6.1.3 step a.1).
    if (!VerifySignedData(cert.signature_algorithm(),
                          cert.tbs_certificate_tlv(), cert.signature_value(),
                          working_public_key_.get())) {
      *shortcircuit_chain_validation = true;
      errors->AddError(cert_errors::kVerifySignedDataFailed);
    }
  }
  if (*shortcircuit_chain_validation)
    return;

  // Check the time range for the certificate's validity, ensuring it is valid
  // at |time|.
  // (RFC 5280 section 6.1.3 step a.2)
  VerifyTimeValidity(cert, time, errors);

  // RFC 5280 section 6.1.3 step a.3 calls for checking the certificate's
  // revocation status here. In this implementation revocation checking is
  // implemented separately from path validation.

  // Verify the certificate's issuer name matches the issuing certificate's
  // subject name. (RFC 5280 section 6.1.3 step a.4)
  if (cert.normalized_issuer() != working_normalized_issuer_name_)
    errors->AddError(cert_errors::kSubjectDoesNotMatchIssuer);

  // Name constraints (RFC 5280 section 6.1.3 step b & c)
  // If certificate i is self-issued and it is not the final certificate in the
  // path, skip this step for certificate i.
  if (!name_constraints_list_.empty() &&
      (!IsSelfIssued(cert) || is_target_cert)) {
    for (const NameConstraints* nc : name_constraints_list_) {
      nc->IsPermittedCert(cert.normalized_subject(), cert.subject_alt_names(),
                          errors);
    }
  }

  // RFC 5280 section 6.1.3 step d - f.
  VerifyPolicies(cert, is_target_cert, errors);

  // The key purpose is checked not just for the end-entity certificate, but
  // also interpreted as a constraint when it appears in intermediates. This
  // goes beyond what RFC 5280 describes, but is the de-facto standard. See
  // https://wiki.mozilla.org/CA:CertificatePolicyV2.1#Frequently_Asked_Questions
  VerifyExtendedKeyUsage(cert, required_key_purpose, errors);
}

void PathVerifier::PrepareForNextCertificate(const ParsedCertificate& cert,
                                             CertErrors* errors) {
  // RFC 5280 section 6.1.4 step a-b
  VerifyPolicyMappings(cert, errors);

  // From RFC 5280 section 6.1.4 step c:
  //
  //    Assign the certificate subject name to working_normalized_issuer_name.
  working_normalized_issuer_name_ = cert.normalized_subject();

  // From RFC 5280 section 6.1.4 step d:
  //
  //    Assign the certificate subjectPublicKey to working_public_key.
  working_public_key_ = ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);

  // Note that steps e and f are omitted as they are handled by
  // the assignment to |working_spki| above. See the definition
  // of |working_spki|.

  // From RFC 5280 section 6.1.4 step g:
  if (cert.has_name_constraints())
    name_constraints_list_.push_back(&cert.name_constraints());

  //     (h)  If certificate i is not self-issued:
  if (!IsSelfIssued(cert)) {
    //         (1)  If explicit_policy is not 0, decrement explicit_policy by
    //              1.
    if (explicit_policy_ > 0)
      explicit_policy_ -= 1;

    //         (2)  If policy_mapping is not 0, decrement policy_mapping by 1.
    if (policy_mapping_ > 0)
      policy_mapping_ -= 1;

    //         (3)  If inhibit_anyPolicy is not 0, decrement inhibit_anyPolicy
    //              by 1.
    if (inhibit_any_policy_ > 0)
      inhibit_any_policy_ -= 1;
  }

  //      (i)  If a policy constraints extension is included in the
  //           certificate, modify the explicit_policy and policy_mapping
  //           state variables as follows:
  if (cert.has_policy_constraints()) {
    //         (1)  If requireExplicitPolicy is present and is less than
    //              explicit_policy, set explicit_policy to the value of
    //              requireExplicitPolicy.
    if (cert.policy_constraints().has_require_explicit_policy &&
        cert.policy_constraints().require_explicit_policy < explicit_policy_) {
      explicit_policy_ = cert.policy_constraints().require_explicit_policy;
    }

    //         (2)  If inhibitPolicyMapping is present and is less than
    //              policy_mapping, set policy_mapping to the value of
    //              inhibitPolicyMapping.
    if (cert.policy_constraints().has_inhibit_policy_mapping &&
        cert.policy_constraints().inhibit_policy_mapping < policy_mapping_) {
      policy_mapping_ = cert.policy_constraints().inhibit_policy_mapping;
    }
  }

  //      (j)  If the inhibitAnyPolicy extension is included in the
  //           certificate and is less than inhibit_anyPolicy, set
  //           inhibit_anyPolicy to the value of inhibitAnyPolicy.
  if (cert.has_inhibit_any_policy() &&
      cert.inhibit_any_policy() < inhibit_any_policy_) {
    inhibit_any_policy_ = cert.inhibit_any_policy();
  }

  // From RFC 5280 section 6.1.4 step k:
  //
  //    If certificate i is a version 3 certificate, verify that the
  //    basicConstraints extension is present and that cA is set to
  //    TRUE.  (If certificate i is a version 1 or version 2
  //    certificate, then the application MUST either verify that
  //    certificate i is a CA certificate through out-of-band means
  //    or reject the certificate.  Conforming implementations may
  //    choose to reject all version 1 and version 2 intermediate
  //    certificates.)
  //
  // This code implicitly rejects non version 3 intermediates, since they
  // can't contain a BasicConstraints extension.
  if (!cert.has_basic_constraints()) {
    errors->AddError(cert_errors::kMissingBasicConstraints);
  } else if (!cert.basic_constraints().is_ca) {
    errors->AddError(cert_errors::kBasicConstraintsIndicatesNotCa);
  }

  // From RFC 5280 section 6.1.4 step l:
  //
  //    If the certificate was not self-issued, verify that
  //    max_path_length is greater than zero and decrement
  //    max_path_length by 1.
  if (!IsSelfIssued(cert)) {
    if (max_path_length_ == 0) {
      errors->AddError(cert_errors::kMaxPathLengthViolated);
    } else {
      --max_path_length_;
    }
  }

  // From RFC 5280 section 6.1.4 step m:
  //
  //    If pathLenConstraint is present in the certificate and is
  //    less than max_path_length, set max_path_length to the value
  //    of pathLenConstraint.
  if (cert.has_basic_constraints() && cert.basic_constraints().has_path_len &&
      cert.basic_constraints().path_len < max_path_length_) {
    max_path_length_ = cert.basic_constraints().path_len;
  }

  // From RFC 5280 section 6.1.4 step n:
  //
  //    If a key usage extension is present, verify that the
  //    keyCertSign bit is set.
  if (cert.has_key_usage() &&
      !cert.key_usage().AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN)) {
    errors->AddError(cert_errors::kKeyCertSignBitNotSet);
  }

  // From RFC 5280 section 6.1.4 step o:
  //
  //    Recognize and process any other critical extension present in
  //    the certificate.  Process any other recognized non-critical
  //    extension present in the certificate that is relevant to path
  //    processing.
  VerifyNoUnconsumedCriticalExtensions(cert, errors);
}

// Checks that if the target certificate has properties that only a CA should
// have (keyCertSign, CA=true, pathLenConstraint), then its other properties
// are consistent with being a CA. If it does, adds errors to |errors|.
//
// This follows from some requirements in RFC 5280 section 4.2.1.9. In
// particular:
//
//    CAs MUST NOT include the pathLenConstraint field unless the cA
//    boolean is asserted and the key usage extension asserts the
//    keyCertSign bit.
//
// And:
//
//    If the cA boolean is not asserted, then the keyCertSign bit in the key
//    usage extension MUST NOT be asserted.
//
// TODO(eroman): Strictly speaking the first requirement is on CAs and not the
// certificate client, so could be skipped.
//
// TODO(eroman): I don't believe Firefox enforces the keyCertSign restriction
// for compatibility reasons. Investigate if we need to similarly relax this
// constraint.
void VerifyTargetCertHasConsistentCaBits(const ParsedCertificate& cert,
                                         CertErrors* errors) {
  // Check if the certificate contains any property specific to CAs.
  bool has_ca_property =
      (cert.has_basic_constraints() &&
       (cert.basic_constraints().is_ca ||
        cert.basic_constraints().has_path_len)) ||
      (cert.has_key_usage() &&
       cert.key_usage().AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));

  // If it "looks" like a CA because it has a CA-only property, then check that
  // it sets ALL the properties expected of a CA.
  if (has_ca_property) {
    bool success = cert.has_basic_constraints() &&
                   cert.basic_constraints().is_ca &&
                   (!cert.has_key_usage() ||
                    cert.key_usage().AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
    if (!success) {
      // TODO(eroman): Add DER for basic constraints and key usage.
      errors->AddError(cert_errors::kTargetCertInconsistentCaBits);
    }
  }
}

void PathVerifier::WrapUp(const ParsedCertificate& cert, CertErrors* errors) {
  // From RFC 5280 section 6.1.5:
  //      (a)  If explicit_policy is not 0, decrement explicit_policy by 1.
  if (explicit_policy_ > 0)
    explicit_policy_ -= 1;

  //      (b)  If a policy constraints extension is included in the
  //           certificate and requireExplicitPolicy is present and has a
  //           value of 0, set the explicit_policy state variable to 0.
  if (cert.has_policy_constraints() &&
      cert.policy_constraints().has_require_explicit_policy &&
      cert.policy_constraints().require_explicit_policy == 0) {
    explicit_policy_ = 0;
  }

  // Note step c-e are omitted as the verification function does
  // not output the working public key.

  // From RFC 5280 section 6.1.5 step f:
  //
  //    Recognize and process any other critical extension present in
  //    the certificate n.  Process any other recognized non-critical
  //    extension present in certificate n that is relevant to path
  //    processing.
  //
  // Note that this is duplicated by PrepareForNextCertificate() so as to
  // directly match the procedures in RFC 5280's section 6.1.
  VerifyNoUnconsumedCriticalExtensions(cert, errors);

  // RFC 5280 section 6.1.5 step g is skipped, as the intersection of valid
  // policies was computed during previous steps.
  //
  //    If either (1) the value of explicit_policy variable is greater than
  //    zero or (2) the valid_policy_tree is not NULL, then path processing
  //   has succeeded.
  if (!(explicit_policy_ > 0 || !valid_policy_tree_.IsNull())) {
    errors->AddError(cert_errors::kNoValidPolicy);
  }

  // The following check is NOT part of RFC 5280 6.1.5's "Wrap-Up Procedure",
  // however is implied by RFC 5280 section 4.2.1.9.
  VerifyTargetCertHasConsistentCaBits(cert, errors);

  // Check the public key for the target certificate. The public key for the
  // other certificates is already checked by PrepareForNextCertificate().
  // Note that this step is not part of RFC 5280 6.1.5.
  ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);
}

void PathVerifier::ApplyTrustAnchorConstraints(const ParsedCertificate& cert,
                                               KeyPurpose required_key_purpose,
                                               CertErrors* errors) {
  // This is not part of RFC 5937 nor RFC 5280, but matches the EKU handling
  // done for intermediates (described in Web PKI's Baseline Requirements).
  VerifyExtendedKeyUsage(cert, required_key_purpose, errors);

  // The following enforcements follow from RFC 5937 (primarily section 3.2):

  // Initialize name constraints initial-permitted/excluded-subtrees.
  if (cert.has_name_constraints())
    name_constraints_list_.push_back(&cert.name_constraints());

  // TODO(eroman): Initialize user-initial-policy-set based on anchor
  // constraints.

  // TODO(eroman): Initialize inhibit any policy based on anchor constraints.

  // TODO(eroman): Initialize require explicit policy based on anchor
  // constraints.

  // TODO(eroman): Initialize inhibit policy mapping based on anchor
  // constraints.

  // From RFC 5937 section 3.2:
  //
  //    If a basic constraints extension is associated with the trust
  //    anchor and contains a pathLenConstraint value, set the
  //    max_path_length state variable equal to the pathLenConstraint
  //    value from the basic constraints extension.
  //
  // NOTE: RFC 5937 does not say to enforce the CA=true part of basic
  // constraints.
  if (cert.has_basic_constraints() && cert.basic_constraints().has_path_len)
    max_path_length_ = cert.basic_constraints().path_len;

  // From RFC 5937 section 2:
  //
  //    Extensions may be marked critical or not critical.  When trust anchor
  //    constraints are enforced, clients MUST reject certification paths
  //    containing a trust anchor with unrecognized critical extensions.
  VerifyNoUnconsumedCriticalExtensions(cert, errors);
}

void PathVerifier::ProcessRootCertificate(const ParsedCertificate& cert,
                                          const CertificateTrust& trust,
                                          KeyPurpose required_key_purpose,
                                          CertErrors* errors,
                                          bool* shortcircuit_chain_validation) {
  *shortcircuit_chain_validation = false;
  switch (trust.type) {
    case CertificateTrustType::UNSPECIFIED:
      // Doesn't chain to a trust anchor - implicitly distrusted
      errors->AddError(cert_errors::kCertIsNotTrustAnchor);
      *shortcircuit_chain_validation = true;
      break;
    case CertificateTrustType::DISTRUSTED:
      // Chains to an actively distrusted certificate.
      errors->AddError(cert_errors::kDistrustedByTrustStore);
      *shortcircuit_chain_validation = true;
      break;
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
      // If the trust anchor has constraints, enforce them.
      if (trust.type == CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS) {
        ApplyTrustAnchorConstraints(cert, required_key_purpose, errors);
      }
      break;
  }
  if (*shortcircuit_chain_validation)
    return;

  // Use the certificate's SPKI and subject when verifying the next certificate.
  working_public_key_ = ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);
  working_normalized_issuer_name_ = cert.normalized_subject();
}

bssl::UniquePtr<EVP_PKEY> PathVerifier::ParseAndCheckPublicKey(
    const der::Input& spki,
    CertErrors* errors) {
  // Parse the public key.
  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!ParsePublicKey(spki, &pkey)) {
    errors->AddError(cert_errors::kFailedParsingSpki);
    return nullptr;
  }

  // Check if the key is acceptable by the delegate.
  if (!delegate_->IsPublicKeyAcceptable(pkey.get(), errors))
    errors->AddError(cert_errors::kUnacceptablePublicKey);

  return pkey;
}

void PathVerifier::Run(
    const ParsedCertificateList& certs,
    const CertificateTrust& last_cert_trust,
    VerifyCertificateChainDelegate* delegate,
    const der::GeneralizedTime& time,
    KeyPurpose required_key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input>& user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit,
    std::set<der::Input>* user_constrained_policy_set,
    CertPathErrors* errors) {
  // This implementation is structured to mimic the description of certificate
  // path verification given by RFC 5280 section 6.1.
  DCHECK(delegate);
  DCHECK(errors);

  delegate_ = delegate;

  // An empty chain is necessarily invalid.
  if (certs.empty()) {
    errors->GetOtherErrors()->AddError(cert_errors::kChainIsEmpty);
    return;
  }

  // Verifying a trusted leaf certificate is not permitted. (It isn't a
  // well-specified operation.) See https://crbug.com/814994.
  if (certs.size() == 1) {
    errors->GetOtherErrors()->AddError(cert_errors::kChainIsLength1);
    return;
  }

  // RFC 5280's "n" variable is the length of the path, which does not count
  // the trust anchor. (Although in practice it doesn't really change behaviors
  // if n is used in place of n+1).
  const size_t n = certs.size() - 1;

  valid_policy_tree_.Init(user_initial_policy_set);

  // RFC 5280 section section 6.1.2:
  //
  // If initial-explicit-policy is set, then the initial value
  // [of explicit_policy] is 0, otherwise the initial value is n+1.
  explicit_policy_ =
      initial_explicit_policy == InitialExplicitPolicy::kTrue ? 0 : n + 1;

  // RFC 5280 section section 6.1.2:
  //
  // If initial-any-policy-inhibit is set, then the initial value
  // [of inhibit_anyPolicy] is 0, otherwise the initial value is n+1.
  inhibit_any_policy_ =
      initial_any_policy_inhibit == InitialAnyPolicyInhibit::kTrue ? 0 : n + 1;

  // RFC 5280 section section 6.1.2:
  //
  // If initial-policy-mapping-inhibit is set, then the initial value
  // [of policy_mapping] is 0, otherwise the initial value is n+1.
  policy_mapping_ =
      initial_policy_mapping_inhibit == InitialPolicyMappingInhibit::kTrue
          ? 0
          : n + 1;

  // RFC 5280 section section 6.1.2:
  //
  // max_path_length:  this integer is initialized to n, ...
  max_path_length_ = n;

  // Iterate over all the certificates in the reverse direction: starting from
  // the root certificate and progressing towards the target certificate.
  //
  //   * i=0  :  Root certificate (i.e. trust anchor)
  //   * i=1  :  Certificate issued by root
  //   * i=x  :  Certificate i=x is issued by certificate i=x-1
  //   * i=n  :  Target certificate.
  for (size_t i = 0; i < certs.size(); ++i) {
    const size_t index_into_certs = certs.size() - i - 1;

    // |is_target_cert| is true if the current certificate is the target
    // certificate being verified. The target certificate isn't necessarily an
    // end-entity certificate.
    const bool is_target_cert = index_into_certs == 0;
    const bool is_root_cert = i == 0;

    const ParsedCertificate& cert = *certs[index_into_certs];

    // Output errors for the current certificate into an error bucket that is
    // associated with that certificate.
    CertErrors* cert_errors = errors->GetErrorsForCert(index_into_certs);

    if (is_root_cert) {
      bool shortcircuit_chain_validation = false;
      ProcessRootCertificate(cert, last_cert_trust, required_key_purpose,
                             cert_errors, &shortcircuit_chain_validation);
      if (shortcircuit_chain_validation) {
        // Chains that don't start from a trusted root should short-circuit the
        // rest of the verification, as accumulating more errors from untrusted
        // certificates would not be meaningful.
        DCHECK(cert_errors->ContainsAnyErrorWithSeverity(
            CertError::SEVERITY_HIGH));
        return;
      }

      // Don't do any other checks for root certificates.
      continue;
    }

    bool shortcircuit_chain_validation = false;
    // Per RFC 5280 section 6.1:
    //  * Do basic processing for each certificate
    //  * If it is the last certificate in the path (target certificate)
    //     - Then run "Wrap up"
    //     - Otherwise run "Prepare for Next cert"
    BasicCertificateProcessing(cert, is_target_cert, time, required_key_purpose,
                               cert_errors, &shortcircuit_chain_validation);
    if (shortcircuit_chain_validation) {
      // Signature errors should short-circuit the rest of the verification, as
      // accumulating more errors from untrusted certificates would not be
      // meaningful.
      DCHECK(
          cert_errors->ContainsAnyErrorWithSeverity(CertError::SEVERITY_HIGH));
      return;
    }
    if (!is_target_cert) {
      PrepareForNextCertificate(cert, cert_errors);
    } else {
      WrapUp(cert, cert_errors);
    }
  }

  if (user_constrained_policy_set) {
    // valid_policy_tree_ already contains the intersection of valid policies
    // with user_initial_policy_set.
    valid_policy_tree_.GetValidRootPolicySet(user_constrained_policy_set);
  }

  // TODO(eroman): RFC 5280 forbids duplicate certificates per section 6.1:
  //
  //    A certificate MUST NOT appear more than once in a prospective
  //    certification path.
}

}  // namespace

VerifyCertificateChainDelegate::~VerifyCertificateChainDelegate() = default;

void VerifyCertificateChain(
    const ParsedCertificateList& certs,
    const CertificateTrust& last_cert_trust,
    VerifyCertificateChainDelegate* delegate,
    const der::GeneralizedTime& time,
    KeyPurpose required_key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input>& user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit,
    std::set<der::Input>* user_constrained_policy_set,
    CertPathErrors* errors) {
  PathVerifier verifier;
  verifier.Run(certs, last_cert_trust, delegate, time, required_key_purpose,
               initial_explicit_policy, user_initial_policy_set,
               initial_policy_mapping_inhibit, initial_any_policy_inhibit,
               user_constrained_policy_set, errors);
}

}  // namespace net
