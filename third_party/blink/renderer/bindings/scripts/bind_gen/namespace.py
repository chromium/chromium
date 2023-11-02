# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from .package_initializer import package_initializer
from .task_queue import TaskQueue

# IDL interface and IDL namespace share a lot by their nature, so this module
# uses the implementation of IDL interface.
from .interface import generate_class_like


def generate_namespace(namespace_identifier):
    assert isinstance(namespace_identifier, web_idl.Identifier)

    web_idl_database = package_initializer().web_idl_database()
    namespace = web_idl_database.find(namespace_identifier)

    generate_class_like(namespace)


def generate_namespaces(task_queue):
    assert isinstance(task_queue, TaskQueue)

    web_idl_database = package_initializer().web_idl_database()

    for namespace in web_idl_database.namespaces:
        task_queue.post_task(generate_namespace, namespace.identifier)
