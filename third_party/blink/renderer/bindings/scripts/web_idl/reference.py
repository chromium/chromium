# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import DebugInfo
from .composition_parts import WithIdentifier


class Proxy(object):
    """
    Proxies attribute access on this object to the target object.
    """

    def __init__(self,
                 target_object=None,
                 target_attrs=None,
                 target_attrs_with_priority=None):
        """
        Creates a new proxy to |target_object|.

        Args:
            target_object: The object to which attribute access is proxied.
                This can be set later by set_target_object.
            target_attrs: None or list of attribute names to be proxied.  If
                None, all the attribute access is proxied.
            target_attrs_with_priority: None or list of attribute names to be
                unconditionally proxied with priority over attributes defined on
                |self|.  If None, no attribute has priority over own attributes.
        """
        if target_attrs is not None:
            assert isinstance(target_attrs, (list, set, tuple))
            assert all(isinstance(attr, str) for attr in target_attrs)
        self._target_object = target_object
        self._target_attrs = target_attrs
        self._target_attrs_with_priority = target_attrs_with_priority

    def __getattr__(self, attribute):
        try:
            target_object = object.__getattribute__(self, '_target_object')
            target_attrs = object.__getattribute__(self, '_target_attrs')
        except AttributeError:
            # When unpickling, __init__ does not get called.  _target_object is
            # not defined yet during unpickling.  Then, just fallback to the
            # default access.
            return object.__getattribute__(self, attribute)
        assert target_object is not None
        if target_attrs is None or attribute in target_attrs:
            return getattr(target_object, attribute)
        raise AttributeError

    def __getattribute__(self, attribute):
        try:
            target_object = object.__getattribute__(self, '_target_object')
            target_attrs = object.__getattribute__(
                self, '_target_attrs_with_priority')
        except AttributeError:
            # When unpickling, __init__ does not get called.  _target_object is
            # not defined yet during unpickling.  Then, just fallback to the
            # default access.
            return object.__getattribute__(self, attribute)
        # It's okay to access own attributes, such as 'identifier', even when
        # the target object is not yet resolved.
        if target_object is None:
            return object.__getattribute__(self, attribute)
        if target_attrs is not None and attribute in target_attrs:
            return getattr(target_object, attribute)
        return object.__getattribute__(self, attribute)

    @staticmethod
    def get_all_attributes(target_class):
        """
        Returns all the attributes of |target_class| including its ancestors'
        attributes.  Protected attributes (starting with an underscore,
        including two underscores) are excluded.
        """

        def collect_attrs_recursively(target_class):
            attrs_sets = [set(vars(target_class).keys())]
            for base_class in target_class.__bases__:
                attrs_sets.append(collect_attrs_recursively(base_class))
            return set.union(*attrs_sets)

        assert isinstance(target_class, type)
        return sorted([
            attr for attr in collect_attrs_recursively(target_class)
            if not attr.startswith('_')
        ])

    def make_copy(self, memo):
        return self

    def set_target_object(self, target_object):
        assert self._target_object is None
        assert isinstance(target_object, object)
        self._target_object = target_object

    @property
    def target_object(self):
        assert self._target_object is not None
        return self._target_object


_REF_BY_ID_PASS_KEY = object()


class RefById(Proxy, WithIdentifier):
    """
    Represents a reference to an object specified with the given identifier,
    which reference will be resolved later.

    This reference is also a proxy to the object for convenience so that you
    can treat this reference as if the object itself.
    """

    def __init__(self,
                 identifier,
                 debug_info=None,
                 target_attrs=None,
                 target_attrs_with_priority=None,
                 pass_key=None):
        assert debug_info is None or isinstance(debug_info, DebugInfo)
        assert pass_key is _REF_BY_ID_PASS_KEY

        Proxy.__init__(
            self,
            target_attrs=target_attrs,
            target_attrs_with_priority=target_attrs_with_priority)
        WithIdentifier.__init__(self, identifier)
        self._ref_own_debug_info = debug_info

    @property
    def ref_own_debug_info(self):
        """This reference's own DebugInfo."""
        return self._ref_own_debug_info


class RefByIdFactory(object):
    """
    Creates a group of references that are later resolvable.

    All the references created by this factory are grouped per factory, and you
    can apply a function to all the references.  This allows you to resolve all
    the references at very end of the compilation phases.
    """

    def __init__(self, target_attrs=None, target_attrs_with_priority=None):
        self._references = []
        # |_is_frozen| is initially False and you can create new references.
        # The first invocation of |for_each| freezes the factory and you can no
        # longer create a new reference
        self._is_frozen = False
        self._target_attrs = target_attrs
        self._target_attrs_with_priority = target_attrs_with_priority

    def create(self, identifier, debug_info=None):
        """
        Creates a new instance of RefById.

            Args:
                identifier: An identifier to be resolved later.
                debug_info: Where the reference is created, which is useful
                    especially when the reference is unresolvable.
        """
        assert not self._is_frozen
        ref = RefById(
            identifier,
            debug_info=debug_info,
            target_attrs=self._target_attrs,
            target_attrs_with_priority=self._target_attrs_with_priority,
            pass_key=_REF_BY_ID_PASS_KEY)
        self._references.append(ref)
        return ref

    def init_subclass_instance(self, instance, identifier, debug_info=None):
        """
        Initializes an instance of a subclass of RefById.
        """
        assert type(instance) is not RefById
        assert isinstance(instance, RefById)
        assert not self._is_frozen
        RefById.__init__(
            instance,
            identifier,
            debug_info=debug_info,
            target_attrs=self._target_attrs,
            target_attrs_with_priority=self._target_attrs_with_priority,
            pass_key=_REF_BY_ID_PASS_KEY)
        self._references.append(instance)

    def for_each(self, callback):
        """
        Applies |callback| to all the references created by this factory.

        You can no longer create a new reference.

        Args:
            callback: A callable that takes a reference as only the argument.
                Return value is not used.
        """
        assert callable(callback)
        self._is_frozen = True
        for ref in self._references:
            callback(ref)
