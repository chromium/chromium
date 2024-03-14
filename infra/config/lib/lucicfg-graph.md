# Working with the lucicfg graph

[TOC]

The lucicfg graph is central to the operation of lucicfg. The lucicfg graph is a
directed, acylic graph with nodes containing arbitrary information. All standard
LUCI configuration files and many of our custom outputs that are generated are
based off of information added to the graph by the starlark code that is
executed. Execution proceeds in 3 phases:

1. graph construction

    During this phase, starlark code is executed, starting at the top-level
    script (//infra/config/main.star or //infra/config/dev.star) and proceeding
    through any starlark files that `exec` is called on. Code running in the
    context of a file being executed is free to modify the graph by adding nodes
    and edges. Code that is running in the context of a file being loaded cannot
    make any modifications to the graph. Under no context can the graph be
    examined during this phase. In order for custom nodes to affect generated
    output, it is necessary to install a generator using [lucicfg.generator]
    which can be done during execution or load context.

1. graph checking

    During this phase, lucicfg checks the graph to ensure there are no cycles
    and that both the parent and child nodes exist for any edges that were
    added. No custom code runs during this phase.

1. graph traversal/file generation

    During this phase, generators installed during phase 1 will be executed in
    the order they were installed. The generators receive a context object that
    allows them to access the outputs to modify them or create additional
    outputs. Generators are able to examine the graph but cannot modify it.

## Graph concepts

As far as the starlark that lucicfg executes is concerned, there are 2 relevant
types that make up the lucicfg graph: keys and nodes.

### Keys

A key serves as the identifier for a node. All operations that modify or query
the graph take at least one key. Keys are essentially a wrapper around a
sequence of pairs *[(kind1, id1), (kind2, id2), ... (kindN, idN)]* where the
kind and id values are both just strings. The use of a sequence enables the keys
to reflect the hierarchical nature of the IDs of the entities that are being
defined such as a builder being contained by a bucket and the key for a
builder's node using a sequence that is the same as the sequence for the
bucket's node with an additional pair.

Keys have the following attributes:

* `id` - The id value in the final pair of the wrapped sequence. This pair
  represents the most deeply-nested element in the hierarchy.
* `kind` - The kind value in the final pair of the wrapped sequence.
* `container` - The key formed from the key's wrapped sequence with the final
  pair removed.

As far as lucicfg is concerned, neither the kind nor id values in the sequence
have any particular significance, they are just used for comparisons: the entire
sequence is compared when comparing keys and the final kind value is compared
when filtering on kind in [graph.children] and [graph.parents]. The code that
creates and consumes the node is free to attach whatever significance they wish.
Much of our graph code will use the name of the declaration as the id in the
final pair so that it can be easily accessed via `key.id`. Additionally, if a
node or key is printed out, the kinds will be included, so descriptive kinds are
helpful when debugging.

### Nodes

Nodes store the information about some declaration. All nodes are identified by
a key.

Nodes can be created using [graph.add_node] from the [graph library], but much
of our code relies on the [nodes library] for handling the creation of nodes.

Nodes have the following attributes:

* `key` - The node's key.
* `props` - When adding a node, whether via [graph.add_node] or via the add
  function of one of the [nodes library] node types, the `props` argument can be
  passed a dict containing arbitrary data, in which case this will contain a
  struct initialized from the dict. If `props` isn't specified, then this will
  be None instead. This will hold data for a declaration that isn't part of the
  declaration's identity (e.g. properties and dimensions for a builder).

An edge can be added directed from one node to another using [graph.add_edge]
from the [graph library], which takes the keys of the nodes to add an edge
between. There are no attributes associated with an edge and no object exposed
to starlark representing an edge, they can only be traversed via various
functions in the graph [graph library].

## Libraries

### graph library

The
[graph](https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star)
library is included with lucicfg. It can be used by loading `"graph"` from
`"@stdlib//internal/graph.star"`.

The graph library provides the following methods that are used in our code:

* [graph.key] - Create a key that identifies a node, used when adding a
  node or an edge or getting nodes. Mostly called via the [nodes library].
* [graph.keyset] - Create a keyset, which is a collection of keys of different
  kinds. Keysets are not used by the graph itself, but enables the
  implementation of inline declarations via [unscoped node types].
* [graph.is_keyset] - Check whether an object is a keyset. Used in the
  implementation of [unscoped node types].
* [graph.add_node] - Add a node identified by the given key. Mostly called via
  the [nodes library].
* [graph.add_edge] - Add an edge between the nodes identified by the provided
  keys. Used by most of the custom code that works with the graph.
* [graph.node] - Get the node from the graph identified by the provided key or
  None if no such node exists. Used by some generators.
* [graph.children] - Get the children of a node identified by the provided key,
  with optional filtering of kinds and ordering controls. Used by some
  generators.
* [graph.descendants] - Get the descendants of a node identified by the provided
  key, with an optional visitor function for controlling what nodes are returned
  and optional ordering controls. Used by some generators.
* [graph.parents] - Get the parents of a node identified by the provided key,
  with optional filtering of kinds and ordering controls. Used by some
  generators.

### nodes library

The [nodes](./nodes.star) library is our custom library that builds on top of
the [graph library]. It is used by loading `"nodes"` from `"//lib/nodes.star"`.
The library contains functions for creating node types, which ensure consistent
key usage for the related nodes and reduces verbosity by removing the need to
explicitly create keys when adding or getting a node.

For all node types create by the nodes library, the [keys](#keys) created will
have an initial *(kind, id)* pair with a fixed kind that creates a
[chromium-specific namespace](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/nodes.star?q=symbol:_CHROMIUM_NS_KIND)
to ensure that there are no collisions with keys created by other mechanisms.
The keys created by standard lucicfg code follows the same pattern with a fixed
kind for a [luci-specific namespace](https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/luci/common.star?q=%22LUCI_NS%20%3D%22).
The keys will contain 1 or 2 additional pairs with the kind(s) specified when
creating the node type.

With the exception of link node types, all of the node types have `add`, `get`
and `key` methods and a `kind` attribute with the primary kind of the node type.
All 3 methods take the same number of positional arguments that are used for
forming a key, with the number of arguments being determined by the node type.

* `add` - Add a new node of the node type's kind(s) to the graph, returning the
  key that identifies the node. Can only be called during phase 1. Additional
  keyword arguments can be passed that will be forwarded on to [graph.add_node].
* `get` - Get a node of the node type kind(s) from the graph. Can only be called
  during phase 3.
* `key` - Get a key of the node type's kind(s). Can be used during phase 1 and
  3, but primarily used in phase 1 when adding a node to get a key for another
  node type to create an edge.

#### singleton node types

A singleton node type is used for creating global configuration for generators
since the graph is the only form of non-fixed input a generator can access.

An example of a singleton node type is the [node type](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/chrome_settings.star?q=symbol:_PER_BUILDER_OUTPUTS)
used to hold the root output directory for per-builder outputs that is used by
the generators for bootstrapping, GN args and tests in starlark. The builders
created from //infra/config/main.star and //infra/config/dev.star use separate
directories so that there are no issues with collisions.

A singleton node type only enables adding a single node of the corresponding
kind to the graph. Its `add`, `get` and `key` methods take no positional
arguments.

A singleton node type can be created by calling
[nodes.create_singleton_node_type](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/nodes.star?q=symbol:_create_singleton_node_type).

#### unscoped node types

An unscoped node type is used for creating functions that declare entities that
exist in a single global namespace (e.g. bundles, mixins).

An unscoped node type allows for the creation of 1 node of the corresponding
kind per id, which the user is free to assign any semantics to. Its `add`, `get`
and `key` methods take the id value as a single positional argument.

An unscoped node type can be created by calling
[nodes.create_unscoped_node_type](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/nodes.star?q=symbol:_create_unscoped_node_type).

In addition to specifying the kind, when defining an unscoped node type
`allow_empty_id=True` can be passed. When this is the case, the key value passed
to `add` can be `None`, which will result in a unique value being assigned to
use as the key's id. The intended usage of this is enabling in-line declarations
by returning the key of an added node wrapped in a [graph.keyset]. This works
because the `key` method is updated to also accept a keyset containing a key of
the corresponding kind.

```python
_FOO = nodes.create_unscoped_node_type("foo", allow_empty_id = True)
_BAR = nodes.create_unscoped_node_type("bar")

def foo(*, color, name = None):
    foo_key = _FOO.add(name, props = dict(color = color))
    return graph.keyset(foo_key)

def bar(*, name, foos):
    bar_key = _BAR.add(name)
    for foo in foos:
        foo_key = _FOO.key(foo)
        graph.add_edge(bar_key, foo_key)

foo(
    name = "red-foo",
    color = "red",
)

bar(
    name = "bar",
    foos = [
        # The name of a separately declared foo can be passed because _FOO.key
        # accepts the ID value ...
        "red",
        # ... or an inline declaration can be passed because _FOO.key accepts a
        # keyset containing a key from _FOO.add, which is what foo returns
        foo(
            color = "sky-blue",
        ),
    ]
)
```

#### scoped node types

A scoped node type is used for nodes where 2 values are needed to identify the
declaration. This might be because there isn't a single value in the declaration
that uniquely identifies the declaration (e.g. builders with different buckets
can use the same name) or because the declaration doesn't actually define an
entity, but instead records some data about some entity in the context of
another entity (e.g. per-test modifications can't be identified by just the name
of the test they modify because multiple bundles could modify the same test in
different ways).

A scoped node type allows for the creation of 1 node per combination of 2 ids.
In addition to the primary kind that would be specified for a singleton or
unscoped node type, a scope kind is specified. The keys created for a scoped
node type have pairs added for both the scope kind and the primary kind. Its
`add`, `get` and `key` methods accept 2 positional arguments: the id for the
scope kind and the id for the primary kind.

A scoped node type can be created by calling
[nodes.create_scoped_node_type](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/nodes.star?q=symbol:_create_scoped_node_type).
For the cases where the scope kind is bucket, a thin wrapper
`nodes.create_bucket_scoped_node_type` can be used, which creates a node type
where the name of the bucket should be used as id for the scope kind.

#### node types with builder ref

A node type with builder ref is a specialization of
[scoped node types](#scoped-node-types) where nodes are associated with builders
and would need to be referenced by other declarations (e.g. a builder that sets
`builder_spec` can be referenced in the `mirrors` attribute of another builder).

A node type with builder ref acts mostly the same as a scoped node type with
bucket as the scope kind, but also provides the functionality for resolving a
builder by either the short name (e.g. `"Linux Builder"`) or the
bucket-qualified name (e.g. `"ci/Linux Builder"`). This is accomplished via the
creation of additional nodes for both references with a generated ref kind when
calling `add`.

Additionally, the node type has a `ref_kind` attribute that has the generated
kind of the ref nodes and the following extra methods:

* `add_ref` - Given a key to a node and a string reference to a builder and adds
  an edge from the node associated with the key to the ref node of this node
  type associated with the builder.
* `follow_ref` - Dereferences a ref node to get the underlying node of the node
  type.

A node type with builder ref can be created by calling
[nodes.create_node_type_with_builder_ref](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/nodes.star?q=symbol:_create_node_type_with_builder_ref)

#### link node types

Link node types are used where it would be desired to have potentially multiple
relationships between kinds.

Because there cannot be multiple edges between two nodes and there are no
attributes on edges, link node types themselves act as additional edges so that
different relationships between two nodes can be distinguished. Like edges, the
user doesn't access the link node itself. Instead the link node type provides
the methods for getting the nodes that are related by the link node type.

As an example of where a link node type is needed, in a basic suite, the config
for a test can reference mixins in the `mixins` and/or `remove_mixins` values.
The treatment of a mixin in these two arguments is different, so generators need
to be able to distinguish between those relationships. To do so, regular edges
are used for capturing the `mixins` relationship, while a
[link node type is used](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/targets-internal/nodes.star?q=symbol:_LEGACY_BASIC_SUITE_REMOVE_MIXIN)
for capturing the `remove_mixins` relationship.

A link node type can be created by calling
[nodes.create_link_node_type](https://source.chromium.org/chromium/chromium/src/+/main:infra/config/lib/nodes.star?q=symbol:_create_link_node_type).
In addition to the kind of the node, parent and child node types are provided,
which are used to check the kinds of keys passed to its methods.

* `link` - Create a link node representing a relationship between the provided
  parent and child keys.
* `children` - Get all the nodes of the child node type related to the node
  identified by the provided parent key.
* `parent` - Get all of the nodes of the parent node type related to the node
  identified by the provided child key.

If the child node type is a
[node type with builder ref](#node-types-with-builder-ref), then `link` takes a
child string reference instead of a child key. `children` and `parents` maintain
the same signature but follows through the ref nodes to return the nodes of the
child/parent node type.

## Generators

In order for any custom nodes added by our code to modify file generation or
output additional files, there must be a generator that examines the nodes and
modifies the outputs. Generators are installed with [lucicfg.generator].

Unlike modifications to the graph, a file being loaded is free to install
generators. This means that libraries typically install necessary generators in
the global scope of the file rather than requiring some function from the
library to be called. Generators are run in the order they are installed, so the
ordering is important for generators that impact each other. To ensure that
generators are run in the proper order, libraries will load other libraries
whose generators should run first (e.g. the bootstrap library loads the
builder_config library so that the generator setting the builder config property
runs before the bootstrap generator transfers builder properties to the
properties files).

[lucicfg.generator] receives a single argument that is the implementation
function for the generator. The generator function takes a single argument that
is a context object that provides access to the outputs. The `outputs` attribute
of the context object is a dict that maps the file path of a generated file
(relative to the config directory specified in [lucicfg.config]) to the contents
of the file. The contents of the file can either be a string or a proto object.
In the case of a string, the string is the literal contents of the file. In the
case of a proto object, the file contents will be the serialized form of the
proto. The standard LUCI config files appear in the map as proto files, which
allows our generators to easily inspect them.

## Example

Putting together most of the concepts, here's an example of a library that would
allow for declarations of foo entities with a color property and bar entities
that depend on some number of foo entities. From those declarations, a
bar-colors.json will be generated that lists for each bar instance the colors
associated with it.

```python
load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("//lib/nodes.star", "nodes")

_FOO = nodes.create_unscoped_node_type("foo", allow_empty_id = True)
_BAR = nodes.create_unscoped_node_type("bar")

def foo(*, color, name = None):
    foo_key = _FOO.add(name, props = dict(color = color))
    return graph.keyset(foo_key)

def bar(*, name, foos):
    bar_key = _BAR.add(name)
    # keys.project() identifies the single project node that is added by
    # lucicfg. Using the project node as the parent enables easily registering
    # the set of nodes that the generator should examine when there's no other
    # source to check against.
    graph.add_edge(keys.project(), bar_key)
    for foo in foos:
        foo_key = _FOO.key(foo)
        graph.add_edge(bar_key, foo_key)

def _foobar_generator(ctx):
  output = {}
  for bar_node in graph.children(keys.project(), kind = _BAR.kind):
    colors = []
    # order_by = graph.DEFINITION_ORDER returns the nodes in the order that
    # edges were added from the parent to the children instead of the default of
    # sorting by the children's keys.
    for foo_node in graph.children(keys.project(), kind = _FOO.kind, order_by = graph.DEFINITION_ORDER):
      colors.append(foo_node.props.color)
    output[bar_node.key.id] = colors
  # json.encode returns a string containing the compact json representation of
  # an object, json.indent makes it more human-readable by splitting the
  # representation across multiple lines and adding indentation.
  ctx.output['bar-colors.json'] = json.indent(json.encode(output), indent = "  ")

lucicfg.generator(_foobar_generator)
```

From the following declarations:

```python
foo(
    name = "color-of-money",
    color = "green",
)

foo(
    name = "red-foo",
    color = "red",
)

bar(
    name = "lmfao-bar",
    foos = [
        "red-foo",
        foo(
            color = "sky-blue",
        ),
    ]
)

bar(
    name = "christmas-bar",
    foos = [
        "color-of-money",
        "red-foo",
    ],
)
```

bar-colors.json will be generated with the following contents:

```json
{
  "christmas-bar": [
    "green"
    "red",
  ],
  "lmfao-bar": [
    "red",
    "sky-blue"
  ]
}
```

[lucicfg.config]: https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/lucicfg/doc/README.md#lucicfg.config
[lucicfg.generator]: https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/lucicfg/doc/README.md#lucicfg.generator

[graph library]: #graph-library
[nodes library]: #nodes-library

[graph.key]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_key%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.keyset]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_keyset%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.is_keyset]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_is_keyset%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.add_node]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_add_node%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.add_edge]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_add_edge%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.node]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_node%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.children]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_children%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.descendants]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_descendants%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra
[graph.parents]: https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/lucicfg/starlark/stdlib/internal/graph.star?q=f:graph%5C.star%20symbol:%5Cb_parents%5Cb%20case:yes&ss=chromium%2Finfra%2Finfra

[unscoped node types]: #unscoped-node-types
