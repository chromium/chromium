# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility library for working with lucicfg graph nodes."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/sequence.star", "sequence")
load("@stdlib//internal/luci/common.star", "builder_ref", "keys", "kinds")

_CHROMIUM_NS_KIND = "@chromium"

def _create_singleton_node_type(kind):
    """Create a singleton node type.

    Singleton nodes types only allow for a single node of the type to exist.
    This can be used for creating configuration nodes for generators since
    generators are unable to access lucicfg vars.

    Args:
        kind: (str) An identifier for the kind of the node. Must be unique
            within the chromium namespace.

    Returns:
        A node type that can be used for creating and getting a node of
        the given kind.

        The type has the following properties:
        * kind: The kind of node of the type.

        The node type has the following methods:
        * key(): Creates a key for the node.
        * add(**kwargs): Adds a node with a key created via `key()`.
            `graph.add_node` will be called with the key and `**kwargs`.
            Returns the key.
        * get(): Gets the node with the key given by
            `key(bucket_name, key_value)`.
    """

    def key():
        return graph.key(_CHROMIUM_NS_KIND, "", kind, "")

    def add(**kwargs):
        k = key()
        graph.add_node(k, **kwargs)
        return k

    def get():
        return graph.node(key())

    return struct(
        kind = kind,
        key = key,
        add = add,
        get = get,
    )

_ANONYMOUS_PREFIX = "<anonymous>"

def _create_unscoped_node_type(kind, allow_empty_id = False):
    """Create an unscoped node type.

    Unscoped node types only allow for one node to exist with a given key_value.
    Key values are arbitrary, it is up to the calling code to assign meaning to
    the key values and enforce validity.

    Args:
        kind: (str) An identifier for the kind of the node. Must be unique
            within the chromium namespace.
        allow_empty_id: (bool) Whether or not to allow the creation of nodes
            without providing an ID value. This can allow for creating resources
            that are defined within the definition of other resources without
            requiring assigning an ID upfront. Instead, a unique ID will be
            generated.

    Returns:
        A node type that can be used for creating and getting nodes of
        the given kind.

        The type has the following properties:
        * kind: The kind of nodes of the type.

        The node types has the following methods:
        * key(key_id_or_keyset): Gets a key of kind. key_id_or_keyset can either
            be the ID value for the key or it can be a keyset, in which case the
            key of kind will be extracted from the keyset.
        * add(key_id, **kwargs): Adds a node with a key created via
            `key(key_id)`. `graph.add_node` will be called with the key and
            `**kwargs`. Returns the key. If allow_empty_id is True, key_id will
            have the defult value of None and a None value for key_id will
            create a node with a key that is unique within the lucicfg run.
        * get(key_id): Gets the node with key given by `key(key_id)`.
    """

    def key(key_id_or_keyset):
        if graph.is_keyset(key_id_or_keyset):
            return key_id_or_keyset.get(kind)
        return graph.key(_CHROMIUM_NS_KIND, "", kind, key_id_or_keyset)

    if allow_empty_id:
        def add(key_id = None, **kwargs):
            if key_id == None:
                key_id = "{}:{}".format(_ANONYMOUS_PREFIX, sequence.next(kind))
            elif key_id.startswith(_ANONYMOUS_PREFIX):
                fail("cannot specify a key ID with prefix \"{}\"".format(_ANONYMOUS_PREFIX))
            k = key(key_id)
            graph.add_node(k, **kwargs)
            return k
    else:
        def add(key_id, **kwargs):
            k = key(key_id)
            graph.add_node(k, **kwargs)
            return k

    def get(key_id):
        return graph.node(key(key_id))

    return struct(
        kind = kind,
        key = key,
        add = add,
        get = get,
    )

def _create_scoped_node_type(kind, scope_kind):
    """Create a node type scoped to another kind.

    Scoped node types allow for a node to exist with a given key value per key
    value of the scope kind. Key values are arbitrary, it is up to the calling
    code to assign meaning to the key values and enforce validity.

    Args:
        kind: (str) An identifier for the kind of the node. Must be unique within
            the chromium namespace.
        scope_kind: (str) An identifier for the scope kind.

    Returns:
        A node type that can be used for creating and getting nodes of
        the given kind.

        The type has the following properties:
        * kind: The kind of nodes of the type.

        The node types has the following methods:
        * key(scope_key_value, key_value): Creates a key with the given scope
            value as the container and key_value as the ID.
        * add(scope_key_value, key_value, **kwargs): Adds a node with a key
            created via `key(scope_key_value, key_value)`. `graph.add_node` will
            be called with the key and `**kwargs`. Returns the key.
        * get(scope_key_value, key_value): Gets the node with the key given by
            `key(scope_key_value, key_value)`.
    """

    def key(scope_key_value, key_value):
        return graph.key(_CHROMIUM_NS_KIND, "", scope_kind, scope_key_value, kind, key_value)

    def add(scope_key_value, key_value, **kwargs):
        k = key(scope_key_value, key_value)
        graph.add_node(k, **kwargs)
        return k

    def get(scope_key_value, key_value):
        return graph.node(key(scope_key_value, key_value))

    return struct(
        kind = kind,
        key = key,
        add = add,
        get = get,
    )

def _create_node_type_with_builder_ref(kind):
    """Create a node type that allows reference via builder name.

    Node types created by this function result in the creation of 2 different
    kinds: the target kind (`kind`) and a ref kind. One node of the target kind
    can exist for each builder defined in the project. Additionally, 2 nodes of
    the ref kind will exist that have edges to the node of the target kind. The
    ref nodes provide the means of linking to the node via a ref. A ref is
    either a bucket-scoped builder name (e.g. "ci/linux-builder") or simple
    builder name (e.g. "linux-builder") if the simple builder name is
    unambiguous.

    Args:
        kind: (str) An identifier for the kind of the node. Must be unique
            within the chromium namespace.

    Returns:
        A node type that can be used for creating and getting nodes of
        the given kind.

        The type has the following properties:
        * kind: The kind of nodes of the type.
        * ref_kind: The kind of ref nodes of the type.

        The node types has the following methods:
        * key(bucket_name, builder_name): Creates a key for the target kind with
            the given bucket and builder.
        * ref_key(ref): Creates a key with the ref kind for the given ref.
        * add(bucket_name, builder_name, **kwargs): Adds a node with a key
            created via `key(bucket_name, builder_name)`. `graph.add_node` will
            be called with the key and `**kwargs`. Additionally, two ref nodes
            will be created as parents of the target node, one with the
            bucket-scoped builder name and one with the simple builder name.
            Returns the target key.
        * get(builder_name): Gets the node with the key given by
            `key(bucket_name, builder_name)`.
        * add_ref(key, ref): Add an edge from an arbitrary node identified by
            `key` to one of this node type's ref nodes identified by `ref`.
        * follow_ref(ref_node, context_node): Get the target node that is the
            child of `ref_node`, which is a node of this node type's ref kind.
            In the event that the ref node has multiple children (i.e. a simple
            builder name that is ambiguous) `context_node` will be used in the
            failure message to identify the source of the reference.
    """
    ref_kind = kind + " ref"

    def key(bucket_name, builder_name):
        return graph.key(_CHROMIUM_NS_KIND, "", kinds.BUCKET, bucket_name, kind, builder_name)

    def ref_key(ref):
        chunks = ref.split(":", 1)
        if len(chunks) != 1:
            fail("reference to builder in external project '{}' is not allowed here"
                .format(chunks[0]))
        chunks = ref.split("/", 1)
        if len(chunks) == 1:
            return graph.key("@chromium", "", ref_kind, ref)
        return graph.key("@chromium", "", kinds.BUCKET, chunks[0], ref_kind, chunks[1])

    def add(bucket_name, builder_name, **kwargs):
        k = key(bucket_name, builder_name)
        graph.add_node(k, **kwargs)
        for ref in (builder_name, "{}/{}".format(bucket_name, builder_name)):
            rk = ref_key(ref)
            graph.add_node(rk, idempotent = True)
            graph.add_edge(rk, k)
        return k

    def get(bucket_name, builder_name):
        return graph.node(key(bucket_name, builder_name))

    def add_ref(key, ref):
        rk = ref_key(ref)
        graph.add_edge(key, rk)

    def follow_ref(ref_node, context_node):
        if ref_node.key.kind != ref_kind:
            fail("{} is not {}".format(ref_node, ref_kind))

        variants = graph.children(ref_node.key, kind)
        if not variants:
            fail("{} is unexpectedly unconnected".format(ref_node))

        if len(variants) == 1:
            return variants[0]

        fail(
            "ambiguous reference '{}' in {}, possible variants:\n  {}".format(
                ref_node.key.id,
                context_node,
                "\n  ".join([str(v) for v in variants]),
            ),
            trace = context_node.trace,
        )

    return struct(
        kind = kind,
        ref_kind = ref_kind,
        add = add,
        get = get,
        add_ref = add_ref,
        follow_ref = follow_ref,
    )

# A node-type for access to lucicfg standard builder nodes. It doesn't provide
# full access because it doesn't allow for the creation of new nodes, but it can
# be used as a node type when creating link node types.
_BUILDER = struct(
    kind = kinds.BUILDER,
    ref_kind = kinds.BUILDER_REF,
    add_ref = lambda key, ref: graph.add_edge(key, keys.builder_ref(ref)),
    follow_ref = builder_ref.follow,
)

def _create_link_node_type(kind, parent_node_type, child_node_type):
    """Create a link node type.

    A link node type allows for creating nodes that express a relationship
    between two nodes. The nodes themselves only matter for providing a
    connection between the related nodes and as such, no means are provided for
    interacting directly with the nodes of this kind. The specific meaning of
    the relationship is determined by the caller. Nodes of this type would be
    necessary when there are potentially multiple relationships that could exist
    between given nodes.

    Args:
        kind: (str) An identifier for the kind of the node. Must be unique.
        parent_node_type: (node type) The node type of the nodes that will be
            the parent in the link relationship.
        child_node_type: (node type) The node type of the nodes that will be the
            child in the link relationship.

    Returns:
        A node type that can be used for linking related nodes and retrieving
        the parents or children of the relationship.

        The node types has the following methods:
        * link(parent_key, child_key): Create a link between the nodes
            identified by `parent_key` and `child_key`. The name is an arbitrary
            name that will appear in error messages if there are issues with the
            node.
        * children(parent_key): Get the nodes that are linked by nodes of this
            type as children of the node identified by `parent_key`.
        * parents(child_key): Get the nodes that are linked by nodes of this
            type as parents of the node identified by `child_key`.

        If the child_node_type was created by
        `create_node_type_with_builder_ref`, the returned node type changes in
        the following way:
        * link instead has the signature link(name, parent_key, child_ref). A
            link will be created between the node identified by `parent_key` and
            the ref node for the reference `child_ref`.
        * children will get the target node type by calling `follow_ref`, so
            links to ambiguous refs will cause failures.
    """

    # The keys are not actually used after creation of the link, but may appear
    # in error messages, so we create a unique key with the given name
    def key(name):
        return keys.unique(kind, name)

    parent_kind = parent_node_type.kind
    child_kind = child_node_type.kind

    if hasattr(child_node_type, "ref_kind"):
        def link(parent_key, child_ref):
            k = key(child_ref)
            graph.add_node(k)
            graph.add_edge(parent_key, k)
            child_node_type.add_ref(k, child_ref)

        def children(parent_key):
            if parent_key.kind != parent_kind:
                fail("kind of {} is not {}".format(parent_key, parent_kind))
            parent_node = graph.node(parent_key)
            children = []

            # The unique keys use the string representation of an incrementing
            # number, which makes the ordering of them seem somewhat chaotic, so
            # get the link nodes in definition order
            for link_node in graph.children(parent_key, kind, graph.DEFINITION_ORDER):
                for ref_node in graph.children(link_node.key, child_node_type.ref_kind):
                    children.append(child_node_type.follow_ref(ref_node, parent_node))
            return children

        def parents(child_key):
            if child_key.kind != child_kind:
                fail("kind of {} is not {}".format(child_key, child_kind))
            parents = []
            for ref_node in graph.parents(child_key, child_node_type.ref_kind):
                # The unique keys use the string representation of an
                # incrementing number, which makes the ordering of them seem
                # somewhat chaotic, so get the link nodes in definition order
                for link_node in graph.parents(ref_node.key, kind, graph.DEFINITION_ORDER):
                    parents.extend(graph.parents(link_node.key, parent_kind))
            return parents

    else:
        def link(parent_key, child_key):
            k = key(child_key.id)
            graph.add_node(k)
            graph.add_edge(parent_key, k)
            graph.add_edge(k, child_key)

        def children(parent_key):
            if parent_key.kind != parent_kind:
                fail("kind of {} is not {}".format(parent_key, parent_kind))
            children = []

            # The unique keys use the string representation of an incrementing
            # number, which makes the ordering of them seem somewhat chaotic, so
            # get the link nodes in definition order
            for link in graph.children(parent_key, kind, graph.DEFINITION_ORDER):
                children.extend(graph.children(link.key, child_kind))
            return children

        def parents(child_key):
            if child_key.kind != child_kind:
                fail("kind of {} is not {}".format(child_key, child_kind))
            parents = []

            # The unique keys use the string representation of an incrementing
            # number, which makes the ordering of them seem somewhat chaotic, so
            # get the link nodes in definition order
            for link in graph.parents(child_key, kind, graph.DEFINITION_ORDER):
                parents.extend(graph.parents(link.key, parent_kind))
            return parents

    return struct(
        link = link,
        children = children,
        parents = parents,
    )

nodes = struct(
    BUILDER = _BUILDER,
    create_singleton_node_type = _create_singleton_node_type,
    create_unscoped_node_type = _create_unscoped_node_type,
    create_scoped_node_type = _create_scoped_node_type,
    create_bucket_scoped_node_type = lambda kind: _create_scoped_node_type(kind, kinds.BUCKET),
    create_node_type_with_builder_ref = _create_node_type_with_builder_ref,
    create_link_node_type = _create_link_node_type,
)
