# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import style_format
from .codegen_tracing import CodeGenTracing
from .path_manager import PathManager


def init(**kwargs):
    """
    Initializes this package.  See PackageInitializer.__init__ for details
    about the arguments.
    """
    the_instance = PackageInitializer(**kwargs)
    the_instance.init()
    assert the_instance is PackageInitializer.the_instance()


def package_initializer():
    """
    Returns the instance of PackageInitializer that actually initialized this
    package.
    """
    the_instance = PackageInitializer.the_instance()
    assert the_instance
    return the_instance


class PackageInitializer(object):
    """
    PackageInitializer is designed to support 'multiprocessing' package so that
    users can initialize this package in another process with the same
    settings.

    When the 'start method' of 'multiprocessing' package is 'spawn', the global
    environment (e.g. module variables, class variables, etc.) will not be
    inherited.  See also https://docs.python.org/3/library/multiprocessing.html

    PackageInitializer helps reproduce the same runtime environment of this
    process in other processes.  PackageInitializer.init() initializes this
    package in the same way as it was originally initialized iff the current
    process' runtime environment has not yet been initialized.  In other words,
    PackageInitializer.init() works with any start method of multiprocessing
    package.
    """

    # The instance of PackageInitializer that actually initialized this
    # package.
    _the_instance = None

    # The instance of web_idl.Database.
    _the_web_idl_database = None

    @classmethod
    def the_instance(cls):
        return cls._the_instance

    def __init__(self, web_idl_database_path, root_src_dir, root_gen_dir,
                 component_reldirs, enable_style_format,
                 enable_code_generation_tracing):
        """
        Args:
            web_idl_database_path: File path to the web_idl.Database.
            root_src_dir: Project's root directory, which corresponds to "//"
                in GN.
            root_gen_dir: Root directory of generated files, which corresponds
                to "//out/Default/gen" in GN.
            component_reldirs: Pairs of component and output directory.
            enable_style_format: Enable style formatting of the generated
                files.
            enable_code_generation_tracing: Enable tracing of code generation
                to see which Python code generates which line of generated
                code.
        """

        self._web_idl_database_path = web_idl_database_path
        self._root_src_dir = root_src_dir
        self._root_gen_dir = root_gen_dir
        self._component_reldirs = component_reldirs
        self._enable_style_format = enable_style_format
        self._enable_code_generation_tracing = enable_code_generation_tracing

    def init(self):
        if PackageInitializer._the_instance:
            return
        PackageInitializer._the_instance = self

        self._init()

    def _init(self):
        # Load the web_idl.Database as a global object so that every worker or
        # every function running in a worker of 'multiprocessing' does not need
        # to load it.
        PackageInitializer._the_web_idl_database = (
            web_idl.Database.read_from_file(self._web_idl_database_path))

        PathManager.init(
            root_src_dir=self._root_src_dir,
            root_gen_dir=self._root_gen_dir,
            component_reldirs=self._component_reldirs)

        style_format.init(root_src_dir=self._root_src_dir,
                          enable_style_format=self._enable_style_format)

        if self._enable_code_generation_tracing:
            CodeGenTracing.enable_code_generation_tracing()
            # The following Python modules are generally not interesting, so
            # skip the functions in the modules.
            from . import code_node
            from . import code_node_cxx
            from . import codegen_utils
            CodeGenTracing.add_modules_to_be_ignored([
                code_node,
                code_node_cxx,
                codegen_utils,
            ])

    def web_idl_database(self):
        """Returns the global instance of web_idl.Database."""
        assert isinstance(PackageInitializer._the_web_idl_database,
                          web_idl.Database)
        return PackageInitializer._the_web_idl_database
