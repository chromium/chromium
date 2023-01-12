#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set of helpers to generate signed X.509v3 certificates.

This works by shelling out calls to the 'openssl req' and 'openssl ca'
commands, and passing the appropriate command line flags and configuration file
(.cnf).
"""

import base64
import hashlib
import os
import shutil
import subprocess
import sys

from . import openssl_conf

# Enum for the "type" of certificate that is to be created. This is used to
# select sane defaults for the .cnf file and command line flags, but they can
# all be overridden.
TYPE_CA = 2
TYPE_END_ENTITY = 3

# March 1st, 2015 12:00 UTC
MARCH_1_2015_UTC = '150301120000Z'

# March 2nd, 2015 12:00 UTC
MARCH_2_2015_UTC = '150302120000Z'

# January 1st, 2015 12:00 UTC
JANUARY_1_2015_UTC = '150101120000Z'

# September 1st, 2015 12:00 UTC
SEPTEMBER_1_2015_UTC = '150901120000Z'

# January 1st, 2016 12:00 UTC
JANUARY_1_2016_UTC = '160101120000Z'

# October 5th, 2021 12:00 UTC
OCTOBER_5_2021_UTC = '211005120000Z'

# October 5th, 2022 12:00 UTC
OCTOBER_5_2022_UTC = '221005120000Z'

KEY_PURPOSE_ANY = 'anyExtendedKeyUsage'
KEY_PURPOSE_SERVER_AUTH = 'serverAuth'
KEY_PURPOSE_CLIENT_AUTH = 'clientAuth'

DEFAULT_KEY_PURPOSE = KEY_PURPOSE_SERVER_AUTH

# Counters used to generate unique (but readable) path names.
g_cur_path_id = {}

# Output paths used:
#   - g_tmp_dir: where any temporary files (cert req, signing db etc) are
#                saved to.

# See init() for how these are assigned.
g_tmp_dir = None
g_invoking_script_path = None

# The default validity range of generated certificates. Can be modified with
# set_default_validity_range(). This range is intentionally already expired to
# avoid tests being added which depend on the certs being valid at the current
# time rather than specifying the time as an input of the test.
g_default_start_date = OCTOBER_5_2021_UTC
g_default_end_date = OCTOBER_5_2022_UTC


def set_default_validity_range(start_date, end_date):
  """Sets the validity range that will be used for certificates created with
  Certificate"""
  global g_default_start_date
  global g_default_end_date
  g_default_start_date = start_date
  g_default_end_date = end_date


def get_unique_path_id(name):
  """Returns a base filename that contains 'name', but is unique to the output
  directory"""
  # Use case-insensitive matching for counting duplicates, since some
  # filesystems are case insensitive, but case preserving.
  lowercase_name = name.lower()
  path_id = g_cur_path_id.get(lowercase_name, 0)
  g_cur_path_id[lowercase_name] = path_id + 1

  # Use a short and clean name for the first use of this name.
  if path_id == 0:
    return name

  # Otherwise append the count to make it unique.
  return '%s_%d' % (name, path_id)


def get_path_in_tmp_dir(name, suffix):
  return os.path.join(g_tmp_dir, '%s%s' % (name, suffix))


class Key(object):
  """Describes a public + private key pair. It is a dumb wrapper around an
  on-disk key."""

  def __init__(self, path):
    self.path = path


  def get_path(self):
    """Returns the path to a file that contains the key contents."""
    return self.path


def get_or_generate_key(generation_arguments, path):
  """Helper function to either retrieve a key from an existing file |path|, or
  generate a new one using the command line |generation_arguments|."""

  generation_arguments_str = ' '.join(generation_arguments)

  # If the file doesn't already exist, generate a new key using the generation
  # parameters.
  if not os.path.isfile(path):
    key_contents = subprocess.check_output(generation_arguments, text=True)

    # Prepend the generation parameters to the key file.
    write_string_to_file(generation_arguments_str + '\n' + key_contents,
                         path)
  else:
    # If the path already exists, confirm that it is for the expected key type.
    first_line = read_file_to_string(path).splitlines()[0]
    if first_line != generation_arguments_str:
      sys.stderr.write(('\nERROR: The existing key file:\n  %s\nis not '
           'compatible with the requested parameters:\n  "%s" vs "%s".\n'
           'Delete the file if you want to re-generate it with the new '
           'parameters, otherwise pick a new filename\n') % (
               path, first_line, generation_arguments_str))
      sys.exit(1)

  return Key(path)


def get_or_generate_rsa_key(size_bits, path):
  """Retrieves an existing key from a file if the path exists. Otherwise
  generates an RSA key with the specified bit size and saves it to the path."""
  return get_or_generate_key(['openssl', 'genrsa', str(size_bits)], path)


def get_or_generate_ec_key(named_curve, path):
  """Retrieves an existing key from a file if the path exists. Otherwise
  generates an EC key with the specified named curve and saves it to the
  path."""
  return get_or_generate_key(['openssl', 'ecparam', '-name', named_curve,
                              '-genkey'], path)


def create_key_path(base_name):
  """Generates a name that contains |base_name| in it, and is relative to the
  "keys/" directory. If create_key_path(xxx) is called more than once during
  the script run, a suffix will be added."""

  # Save keys to CWD/keys/*.key
  keys_dir = 'keys'

  # Create the keys directory if it doesn't exist
  if not os.path.exists(keys_dir):
    os.makedirs(keys_dir)

  return get_unique_path_id(os.path.join(keys_dir, base_name)) + '.key'


class Certificate(object):
  """Helper for building an X.509 certificate."""

  def __init__(self, name, cert_type, issuer):
    # The name will be used for the subject's CN, and also as a component of
    # the temporary filenames to help with debugging.
    self.name = name
    self.path_id = get_unique_path_id(name)

    # Allow the caller to override the key later. If no key was set will
    # auto-generate one.
    self.key = None

    # The issuer is also a Certificate object. Passing |None| means it is a
    # self-signed certificate.
    self.issuer = issuer
    if issuer is None:
      self.issuer = self

    # The config contains all the OpenSSL options that will be passed via a
    # .cnf file. Set up defaults.
    self.config = openssl_conf.Config()
    self.init_config()

    # Some settings need to be passed as flags rather than in the .cnf file.
    # Technically these can be set though a .cnf, however doing so makes it
    # sticky to the issuing certificate, rather than selecting it per
    # subordinate certificate.
    self.validity_flags = []
    self.md_flags = []

    # By default OpenSSL will use the current time for the start time. Instead
    # default to using a fixed timestamp for more predictable results each time
    # the certificates are re-generated.
    self.set_validity_range(g_default_start_date, g_default_end_date)

    # Use SHA-256 when THIS certificate is signed (setting it in the
    # configuration would instead set the hash to use when signing other
    # certificates with this one).
    self.set_signature_hash('sha256')

    # Set appropriate key usages and basic constraints. For flexibility in
    # testing (since want to generate some flawed certificates) these are set
    # on a per-certificate basis rather than automatically when signing.
    if cert_type == TYPE_END_ENTITY:
      self.get_extensions().set_property('keyUsage',
              'critical,digitalSignature,keyEncipherment')
      self.get_extensions().set_property('extendedKeyUsage',
              'serverAuth,clientAuth')
    else:
      self.get_extensions().set_property('keyUsage',
              'critical,keyCertSign,cRLSign')
      self.get_extensions().set_property('basicConstraints', 'critical,CA:true')

    # Tracks whether the PEM file for this certificate has been written (since
    # generation is done lazily).
    self.finalized = False

    # Initialize any files that will be needed if this certificate is used to
    # sign other certificates. Picks a pseudo-random starting serial number
    # based on the file system path, and will increment this for each signed
    # certificate.
    if not os.path.exists(self.get_serial_path()):
      write_string_to_file('%s\n' % self.make_serial_number(),
                           self.get_serial_path())
    if not os.path.exists(self.get_database_path()):
      write_string_to_file('', self.get_database_path())


  def set_validity_range(self, start_date, end_date):
    """Sets the Validity notBefore and notAfter properties for the
    certificate"""
    self.validity_flags = ['-startdate', start_date, '-enddate', end_date]


  def set_signature_hash(self, md):
    """Sets the hash function that will be used when signing this certificate.
    Can be sha1, sha256, sha512, md5, etc."""
    self.md_flags = ['-md', md]


  def get_extensions(self):
    return self.config.get_section('req_ext')


  def get_subject(self):
    """Returns the configuration section responsible for the subject of the
    certificate. This can be used to alter the subject to be more complex."""
    return self.config.get_section('req_dn')


  def get_path(self, suffix):
    """Forms a path to an output file for this certificate, containing the
    indicated suffix. The certificate's name will be used as its basis."""
    return os.path.join(g_tmp_dir, '%s%s' % (self.path_id, suffix))


  def get_name_path(self, suffix):
    """Forms a path to an output file for this CA, containing the indicated
    suffix. If multiple certificates have the same name, they will use the same
    path."""
    return get_path_in_tmp_dir(self.name, suffix)


  def set_key(self, key):
    assert self.finalized is False
    self.set_key_internal(key)


  def set_key_internal(self, key):
    self.key = key

    # Associate the private key with the certificate.
    section = self.config.get_section('root_ca')
    section.set_property('private_key', self.key.get_path())


  def get_key(self):
    if self.key is None:
      self.set_key_internal(
          get_or_generate_rsa_key(2048, create_key_path(self.name)))
    return self.key


  def get_cert_path(self):
    return self.get_path('.pem')


  def get_serial_path(self):
    return self.get_name_path('.serial')


  def make_serial_number(self):
    """Returns a hex number that is generated based on the certificate file
    path. This serial number will likely be globally unique, which makes it
    easier to use the certificates with NSS (which assumes certificate
    equivalence based on issuer and serial number)."""

    # Hash some predictable values together to get the serial number. The
    # predictability is so that re-generating certificate chains is
    # a no-op, however each certificate ends up with a unique serial number.
    m = hashlib.sha1()

    # Mix in up to the last 3 components of the path for the generating script.
    # For example,
    # "verify_certificate_chain_unittest/my_test/generate_chains.py"
    script_path = os.path.realpath(g_invoking_script_path)
    script_path = "/".join(script_path.split(os.sep)[-3:])
    m.update(script_path.encode('utf-8'))

    # Mix in the path_id, which corresponds to a unique path for the
    # certificate under out/ (and accounts for non-unique certificate names).
    m.update(self.path_id.encode('utf-8'))

    serial_bytes = bytearray(m.digest())

    # SHA1 digest is 20 bytes long, which is appropriate for a serial number.
    # However, need to also make sure the most significant bit is 0 so it is
    # not a "negative" number.
    serial_bytes[0] = serial_bytes[0] & 0x7F

    return serial_bytes.hex()


  def get_csr_path(self):
    return self.get_path('.csr')


  def get_database_path(self):
    return self.get_name_path('.db')


  def get_config_path(self):
    return self.get_path('.cnf')


  def get_cert_pem(self):
    # Finish generating a .pem file for the certificate.
    self.finalize()

    # Read the certificate data.
    return read_file_to_string(self.get_cert_path())


  def finalize(self):
    """Finishes the certificate creation process. This generates any needed
    key, creates and signs the CSR. On completion the resulting PEM file can be
    found at self.get_cert_path()"""

    if self.finalized:
      return # Already finalized, no work needed.

    self.finalized = True

    # Ensure that the issuer has been "finalized", since its outputs need to be
    # accessible. Note that self.issuer could be the same as self.
    self.issuer.finalize()

    # Ensure the certificate has a key (gets lazily created by this call if
    # missing).
    self.get_key()

    # Serialize the config to a file.
    self.config.write_to_file(self.get_config_path())

    # Create a CSR.
    subprocess.check_call(
        ['openssl', 'req', '-new',
         '-key', self.key.get_path(),
         '-out', self.get_csr_path(),
         '-config', self.get_config_path()])

    cmd = ['openssl', 'ca', '-batch', '-in',
        self.get_csr_path(), '-out', self.get_cert_path(), '-config',
        self.issuer.get_config_path()]

    if self.issuer == self:
      cmd.append('-selfsign')

    # Add in any extra flags.
    cmd.extend(self.validity_flags)
    cmd.extend(self.md_flags)

    # Run the 'openssl ca' command.
    subprocess.check_call(cmd)


  def init_config(self):
    """Initializes default properties in the certificate .cnf file that are
    generic enough to work for all certificates (but can be overridden later).
    """

    # --------------------------------------
    # 'req' section
    # --------------------------------------

    section = self.config.get_section('req')

    section.set_property('encrypt_key', 'no')
    section.set_property('utf8', 'yes')
    section.set_property('string_mask', 'utf8only')
    section.set_property('prompt', 'no')
    section.set_property('distinguished_name', 'req_dn')
    section.set_property('req_extensions', 'req_ext')

    # --------------------------------------
    # 'req_dn' section
    # --------------------------------------

    # This section describes the certificate subject's distinguished name.

    section = self.config.get_section('req_dn')
    section.set_property('commonName', '"%s"' % (self.name))

    # --------------------------------------
    # 'req_ext' section
    # --------------------------------------

    # This section describes the certificate's extensions.

    section = self.config.get_section('req_ext')
    section.set_property('subjectKeyIdentifier', 'hash')

    # --------------------------------------
    # SECTIONS FOR CAs
    # --------------------------------------

    # The following sections are used by the 'openssl ca' and relate to the
    # signing operation. They are not needed for end-entity certificate
    # configurations, but only if this certifiate will be used to sign other
    # certificates.

    # --------------------------------------
    # 'ca' section
    # --------------------------------------

    section = self.config.get_section('ca')
    section.set_property('default_ca', 'root_ca')

    section = self.config.get_section('root_ca')
    section.set_property('certificate', self.get_cert_path())
    section.set_property('new_certs_dir', g_tmp_dir)
    section.set_property('serial', self.get_serial_path())
    section.set_property('database', self.get_database_path())
    section.set_property('unique_subject', 'no')

    # These will get overridden via command line flags.
    section.set_property('default_days', '365')
    section.set_property('default_md', 'sha256')

    section.set_property('policy', 'policy_anything')
    section.set_property('email_in_dn', 'no')
    section.set_property('preserve', 'yes')
    section.set_property('name_opt', 'multiline,-esc_msb,utf8')
    section.set_property('cert_opt', 'ca_default')
    section.set_property('copy_extensions', 'copy')
    section.set_property('x509_extensions', 'signing_ca_ext')
    section.set_property('default_crl_days', '30')
    section.set_property('crl_extensions', 'crl_ext')

    section = self.config.get_section('policy_anything')
    section.set_property('domainComponent', 'optional')
    section.set_property('countryName', 'optional')
    section.set_property('stateOrProvinceName', 'optional')
    section.set_property('localityName', 'optional')
    section.set_property('organizationName', 'optional')
    section.set_property('organizationalUnitName', 'optional')
    section.set_property('commonName', 'optional')
    section.set_property('emailAddress', 'optional')

    section = self.config.get_section('signing_ca_ext')
    section.set_property('subjectKeyIdentifier', 'hash')
    section.set_property('authorityKeyIdentifier', 'keyid:always')
    section.set_property('authorityInfoAccess', '@issuer_info')
    section.set_property('crlDistributionPoints', '@crl_info')

    section = self.config.get_section('issuer_info')
    section.set_property('caIssuers;URI.0',
                        'http://url-for-aia/%s.cer' % (self.name))

    section = self.config.get_section('crl_info')
    section.set_property('URI.0', 'http://url-for-crl/%s.crl' % (self.name))

    section = self.config.get_section('crl_ext')
    section.set_property('authorityKeyIdentifier', 'keyid:always')
    section.set_property('authorityInfoAccess', '@issuer_info')


def text_data_to_pem(block_header, text_data):
  # b64encode takes in bytes and returns bytes.
  pem_data = base64.b64encode(text_data.encode('utf8')).decode('utf8')
  return '%s\n-----BEGIN %s-----\n%s\n-----END %s-----\n' % (
      text_data, block_header, pem_data, block_header)


def write_chain(description, chain, out_pem):
  """Writes the chain to a .pem file as a series of CERTIFICATE blocks"""

  # Prepend the script name that generated the file to the description.
  test_data = '[Created by: %s]\n\n%s\n' % (sys.argv[0], description)

  # Write the certificate chain to the output file.
  for cert in chain:
    test_data += '\n' + cert.get_cert_pem()

  write_string_to_file(test_data, out_pem)


def write_string_to_file(data, path):
  with open(path, 'w') as f:
    f.write(data)


def read_file_to_string(path):
  with open(path, 'r') as f:
    return f.read()


def init(invoking_script_path):
  """Creates an output directory to contain all the temporary files that may be
  created, as well as determining the path for the final output. These paths
  are all based off of the name of the calling script.
  """

  global g_tmp_dir
  global g_invoking_script_path

  g_invoking_script_path = invoking_script_path

  # The scripts assume to be run from within their containing directory (paths
  # to things like "keys/" are written relative).
  expected_cwd = os.path.realpath(os.path.dirname(invoking_script_path))
  actual_cwd = os.path.realpath(os.getcwd())
  if actual_cwd != expected_cwd:
    sys.stderr.write(
        ('Your current working directory must be that containing the python '
         'scripts:\n%s\nas the script may reference paths relative to this\n')
        % (expected_cwd))
    sys.exit(1)

  # Use an output directory that is a sibling of the invoking script.
  g_tmp_dir = 'out'

  # Ensure the output directory exists and is empty.
  sys.stdout.write('Creating output directory: %s\n' % (g_tmp_dir))
  shutil.rmtree(g_tmp_dir, True)
  os.makedirs(g_tmp_dir)


def create_self_signed_root_certificate(name):
  return Certificate(name, TYPE_CA, None)


def create_intermediate_certificate(name, issuer):
  return Certificate(name, TYPE_CA, issuer)


def create_self_signed_end_entity_certificate(name):
  return Certificate(name, TYPE_END_ENTITY, None)


def create_end_entity_certificate(name, issuer):
  return Certificate(name, TYPE_END_ENTITY, issuer)

init(sys.argv[0])
