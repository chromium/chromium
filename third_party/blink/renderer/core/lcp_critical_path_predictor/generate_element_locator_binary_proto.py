# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Assembles a binary protobuf from a element locator text proto.
"""

import os
import sys

# go up 3 parent directories to //src/third_party/blink
path_to_blink = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 *[os.path.pardir] * 3))

# go up 2 parent directories to //src
path_to_src_root = os.path.join(path_to_blink, *[os.path.pardir] * 2)

# allow importing modules from //src/components/resources/protobufs
sys.path.insert(
    0,
    os.path.normpath(
        os.path.join(path_to_src_root, 'components/resources/protobufs')))

from binary_proto_generator import BinaryProtoGenerator


class ElementLocatorProtoGenerator(BinaryProtoGenerator):
    def ImportProtoModule(self):
        import element_locator_pb2
        globals()['element_locator_pb2'] = element_locator_pb2

    def EmptyProtoInstance(self):
        # pylint: disable=undefined-variable
        # `element_locator_pb2` is placed directly in `globals()`
        return element_locator_pb2.ElementLocator()

    def ProcessPb(self, opts, pb):
        binary_pb_str = pb.SerializeToString()

        outdir = opts.outdir
        if not os.path.exists(outdir):
            os.makedirs(outdir)

        outfile = os.path.join(outdir, opts.outbasename)
        with open(outfile, 'wb') as f:
            f.write(binary_pb_str)


def main():
    generator = ElementLocatorProtoGenerator()
    generator.Run()


if __name__ == '__main__':
    sys.exit(main())
