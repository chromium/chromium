#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path += ['..']

import gencerts

# Generate the keys -- the same key is used between all intermediate certs and
# between all leaf certs.
root_key = gencerts.get_or_generate_rsa_key(2048,
                                            gencerts.create_key_path('root'))
i_key = gencerts.get_or_generate_rsa_key(2048, gencerts.create_key_path('i'))
leaf_key = gencerts.get_or_generate_rsa_key(2048,
                                            gencerts.create_key_path('leaf'))

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')
root.set_key(root_key)
# Preserve the ordering of the distinguished name in CSRs when issuing
# certificates. This must be in the BASE ('ca') section.
root.config.get_section('ca').set_property('preserve', 'yes')
gencerts.write_string_to_file(root.get_cert_pem(), 'root.pem')

## Create intermediate certs

# Intermediate with two organizations as two distinct SETs, ordered O1 and O2
i_o1_o2 = gencerts.create_intermediate_certificate('I1', root)
i_o1_o2.set_key(i_key)
dn = i_o1_o2.get_subject()
dn.clear_properties()
dn.add_property('0.organizationName', 'O1')
dn.add_property('1.organizationName', 'O2')
gencerts.write_string_to_file(i_o1_o2.get_cert_pem(), 'int-o1-o2.pem')

# Intermediate with two organizations as two distinct SETs, ordered O2 and O1
i_o2_o1 = gencerts.create_intermediate_certificate('I2', root)
i_o2_o1.set_key(i_key)
dn = i_o2_o1.get_subject()
dn.clear_properties()
dn.add_property('0.organizationName', 'O2')
dn.add_property('1.organizationName', 'O1')
gencerts.write_string_to_file(i_o2_o1.get_cert_pem(), 'int-o2-o1.pem')

# Intermediate with a single organization name, O3
i_o3 = gencerts.create_intermediate_certificate('I3', root)
i_o3.set_key(i_key)
dn = i_o3.get_subject()
dn.clear_properties()
dn.add_property('organizationName', 'O3')
gencerts.write_string_to_file(i_o3.get_cert_pem(), 'int-o3.pem')

# Intermediate with a single organization name, O1, encoded as BMPString
i_bmp_o1 = gencerts.create_intermediate_certificate('I4', root)
i_bmp_o1.set_key(i_key)
# 2048 = 0x0800, B_ASN1_BMPSTRING
i_bmp_o1.config.get_section('req').set_property('string_mask', 'MASK:2048')
i_bmp_o1.config.get_section('req').set_property('utf8', 'no')
dn = i_bmp_o1.get_subject()
dn.clear_properties()
dn.add_property('organizationName', 'O1')
gencerts.write_string_to_file(i_bmp_o1.get_cert_pem(), 'int-bmp-o1.pem')

# Intermediate with two organizations as a single SET, ordered O1 and O2
i_o1_plus_o2 = gencerts.create_intermediate_certificate('I5', root)
i_o1_plus_o2.set_key(i_key)
dn = i_o1_plus_o2.get_subject()
dn.clear_properties()
dn.add_property('organizationName', 'O1')
dn.add_property('+organizationName', 'O2')
gencerts.write_string_to_file(i_o1_plus_o2.get_cert_pem(), 'int-o1-plus-o2.pem')

# Intermediate with no organization name (not BR compliant)
i_cn = gencerts.create_intermediate_certificate('I6', root)
i_cn.set_key(i_key)
dn = i_cn.get_subject()
dn.clear_properties()
dn.add_property('commonName', 'O1')
gencerts.write_string_to_file(i_cn.get_cert_pem(), 'int-cn.pem')

## Create name-constrained intermediate certs

# Create a name-constrained intermediate that has O1 as a permitted
# organizationName in a directoryName nameConstraint
nc_permit_o1 = gencerts.create_intermediate_certificate('NC1', root)
nc_permit_o1.set_key(i_key)
nc_permit_o1.get_extensions().set_property('nameConstraints', 'critical,@nc')
nc = nc_permit_o1.config.get_section('nc')
nc.add_property('permitted;dirName.1', 'nc_1')
nc_1 = nc_permit_o1.config.get_section('nc_1')
nc_1.add_property('organizationName', 'O1')
gencerts.write_string_to_file(nc_permit_o1.get_cert_pem(),
                              'nc-int-permit-o1.pem')

# Create a name-constrained intermediate that has O1 as a permitted
# organizationName, but encoded as a BMPString within a directoryName
# nameConstraint
nc_permit_bmp_o1 = gencerts.create_intermediate_certificate('NC2', root)
nc_permit_bmp_o1.set_key(i_key)
# 2048 = 0x0800, B_ASN1_BMPSTRING
nc_permit_bmp_o1.config.get_section('req').set_property('string_mask',
                                                        'MASK:2048')
nc_permit_bmp_o1.config.get_section('req').set_property('utf8', 'no')
nc = nc_permit_bmp_o1.config.get_section('nc')
nc.add_property('permitted;dirName.1', 'nc_1')
nc_1 = nc_permit_bmp_o1.config.get_section('nc_1')
nc_1.add_property('organizationName', 'O1')
gencerts.write_string_to_file(nc_permit_bmp_o1.get_cert_pem(),
                              'nc-int-permit-bmp-o1.pem')

# Create a name-constrained intermediate that has O1 as a permitted
# commonName in a directoryName nameConstraint
nc_permit_cn = gencerts.create_intermediate_certificate('NC3', root)
nc_permit_cn.set_key(i_key)
nc_permit_cn.get_extensions().set_property('nameConstraints', 'critical,@nc')
nc = nc_permit_cn.config.get_section('nc')
nc.add_property('permitted;dirName.1', 'nc_1')
nc_1 = nc_permit_cn.config.get_section('nc_1')
nc_1.add_property('commonName', 'O1')
gencerts.write_string_to_file(nc_permit_cn.get_cert_pem(),
                              'nc-int-permit-cn.pem')

# Create a name-constrainted intermediate that has O1 as an excluded
# commonName in a directoryName nameConstraint
nc_exclude_o1 = gencerts.create_intermediate_certificate('NC4', root)
nc_exclude_o1.set_key(i_key)
nc_exclude_o1.get_extensions().set_property('nameConstraints', 'critical,@nc')
nc = nc_exclude_o1.config.get_section('nc')
nc.add_property('excluded;dirName.1', 'nc_1')
nc_1 = nc_exclude_o1.config.get_section('nc_1')
nc_1.add_property('organizationName', 'O1')
gencerts.write_string_to_file(nc_exclude_o1.get_cert_pem(),
                              'nc-int-exclude-o1.pem')

# Create a name-constrained intermediate that does not have a directoryName
# nameConstraint
nc_permit_dns = gencerts.create_intermediate_certificate('NC5', root)
nc_permit_dns.set_key(i_key)
nc_permit_dns.get_extensions().set_property('nameConstraints', 'critical,@nc')
nc = nc_permit_dns.config.get_section('nc')
nc.add_property('permitted;DNS.1', 'test.invalid')
gencerts.write_string_to_file(nc_permit_dns.get_cert_pem(),
                              'nc-int-permit-dns.pem')

# Create a name-constrained intermediate with multiple directoryName
# nameConstraints
nc_permit_o2_o1_o3 = gencerts.create_intermediate_certificate('NC6', root)
nc_permit_o2_o1_o3.set_key(i_key)
nc_permit_o2_o1_o3.get_extensions().set_property('nameConstraints',
                                                 'critical,@nc')
nc = nc_permit_o2_o1_o3.config.get_section('nc')
nc.add_property('permitted;dirName.1', 'nc_1')
nc_1 = nc_permit_o2_o1_o3.config.get_section('nc_1')
nc_1.add_property('organizationName', 'O2')

nc.add_property('permitted;dirName.2', 'nc_2')
nc_2 = nc_permit_o2_o1_o3.config.get_section('nc_2')
nc_2.add_property('organizationName', 'O1')

nc.add_property('permitted;dirName.3', 'nc_3')
nc_3 = nc_permit_o2_o1_o3.config.get_section('nc_3')
nc_3.add_property('organizationName', 'O3')

gencerts.write_string_to_file(nc_permit_o2_o1_o3.get_cert_pem(),
                              'nc-int-permit-o2-o1-o3.pem')

## Create leaf certs (note: The issuer name does not matter for these tests)

# Leaf missing an organization name
leaf_no_o = gencerts.create_end_entity_certificate('L1', root)
leaf_no_o.set_key(leaf_key)
dn = leaf_no_o.get_subject()
dn.clear_properties()
dn.add_property('commonName', 'O1')
gencerts.write_string_to_file(leaf_no_o.get_cert_pem(), 'leaf-no-o.pem')

# Leaf with two organizations as two distinct SETs, ordered O1 and O2
leaf_o1_o2 = gencerts.create_end_entity_certificate('L2', root)
leaf_o1_o2.set_key(leaf_key)
dn = leaf_o1_o2.get_subject()
dn.clear_properties()
dn.add_property('0.organizationName', 'O1')
dn.add_property('1.organizationName', 'O2')
dn.add_property('commonName', 'Leaf')
gencerts.write_string_to_file(leaf_o1_o2.get_cert_pem(), 'leaf-o1-o2.pem')

# Leaf with a single organization name, O1
leaf_o1 = gencerts.create_end_entity_certificate('L3', root)
leaf_o1.set_key(leaf_key)
dn = leaf_o1.get_subject()
dn.clear_properties()
dn.add_property('0.organizationName', 'O1')
dn.add_property('commonName', 'Leaf')
gencerts.write_string_to_file(leaf_o1.get_cert_pem(), 'leaf-o1.pem')

