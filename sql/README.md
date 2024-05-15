# SQLite abstraction layer

[TOC]

## SQLite for system designers

[SQLite](https://www.sqlite.org/) is a
[relational database management system (RDBMS)](https://en.wikipedia.org/wiki/Relational_database#RDBMS)
that [supports most of SQL](https://www.sqlite.org/lang.html).

SQLite is architected as a library that can be embedded in another application,
such as Chrome. SQLite runs in the application's process, and shares its memory
and other resources. This is similar to embedded databases like
[LevelDB](https://github.com/google/leveldb) and
[BerkeleyDB](https://en.wikipedia.org/wiki/Berkeley_DB). By contrast, most
popular RDMBSes, like [PostgreSQL](https://www.postgresql.org/) and
[MySQL](https://www.mysql.com/), are structured as standalone server processes
that accept queries from client processes.

TODO: Explain the process model and locking

TODO: Explain Chrome decisions -- exclusive locking, full per-feature isolation
(separate databases and page caches)


## SQLite for database designers

The section summarizes aspects of SQLite that are relevant to schema and
query design, and may be surprising to readers with prior experience in other
popular SQL database systems, such as
[PostgreSQL](https://www.postgresql.org/) and [MySQL](https://www.mysql.com/).


### Data storage model {#storage-model}

The main bottleneck in SQLite database performance is usually disk I/O. So,
designing schemas that perform well requires understanding how SQLite stores
data on disk.

At a very high level, a SQLite database is a forest of
[B-trees](https://en.wikipedia.org/wiki/B-tree), some of which are
[B+-trees](https://en.wikipedia.org/wiki/B%2B_tree). The database file is an
array of fixed-size pages, where each page stores a B-tree node. The page size
can only be set when a database file is created, and impacts both SQL statement
execution speed, and memory consumption.

The data in each table (usually called *rows*, *records*, or *tuples*) is stored
in a separate B-tree. The data in each index (called *entries*, *records* or
*tuples*) is also stored in a separate B-tree. So, each B-tree is associated
with exactly one table. The [*Indexing* section](#indexing-model) goes into
further details.

Each B-tree node stores multiple tuples of values. The values and their
encodings are described in the [*Value types* section](#data-types).

Tying everything together: The performance of a SQL statement is roughly the
number of database pages touched (read / written) by the statement. These pages
are nodes belonging to the B-trees associated with the tables mentioned in the
statement. The number of pages touched when accessing a B-tree depends on the
B-tree's depth. Each B-tree's depth depends on its record count (number of
records stored in it), and on its node width (how many records fit in a node).


#### Value types {#data-types}

SQLite stores values using
[5 major types](https://www.sqlite.org/datatype3.html), which are summarized
below.

1. NULL is a special type for the `NULL` value.

2. INTEGER represents big-endian twos-complement integers. Boolean values
   (`TRUE` and `FALSE`) are represented as the integer values 1 and 0.

3. REAL represents IEEE 754-2008 64-bit floating point numbers.

4. TEXT represents strings (sequences of characters) encoded using a
   [supported SQLite encoding](https://www.sqlite.org/c3ref/c_any.html). These
   values are
   [sorted](https://www.sqlite.org/datatype3.html#sort_order) according to
   [a collating sequence](https://www.sqlite.org/datatype3.html#collation) or
   [a collating function](https://www.sqlite.org/c3ref/create_collation.html).

5. BLOB represents sequences of bytes that are opaque to SQLite. These values are
   sorted using the bitwise binary comparison offered by
   [memcmp](https://en.cppreference.com/w/cpp/string/byte/memcmp).

SQLite stores index keys and row values (records / tuples) using
[a tightly packed format](https://sqlite.org/fileformat2.html#record_format)
that makes heavy use of [varints](https://sqlite.org/fileformat2.html#varint)
and variable-length fields. The column types have almost no influence on the
encoding of values. This has the following consequences.

* All SQL integer types, such as `TINYINT` and `BIGINT`, are treated as aliases
  for `INTEGER`.
* All SQL non-integer numeric types, such as `DECIMAL`, `FLOAT`, and
  `DOUBLE PRECISION` are treated as aliases for `REAL`.
* Numeric precision and scale specifiers, such as `DECIMAL(5,2)` are ignored.
* All string types, such as `CHAR`, `CHARACTER VARYING`, `VARCHAR`, and `CLOB`,
  are treated as aliases for `TEXT`.
* Maximum string length specifiers, such as `CHAR(255)` are ignored.

SQLite uses clever heuristics, called
[type affinity](https://www.sqlite.org/datatype3.html#type_affinity),
to map SQL column types such as `VARCHAR` to the major types above.

Chrome database schemas should avoid type affinity, and should not include any
information ignored by SQLite.


#### Indexing {#indexing-model}

SQLite [uses B-trees](https://www.sqlite.org/fileformat2.html#pages) to store
both table and index data.

The exclusive use of B-trees reduces the amount of schema design decisions.
Notable examples:

* There is no equivalent to
  [PostgreSQL's index types](https://www.postgresql.org/docs/13/indexes-types.html).
  In particular, since there are no hashed indexes, the design does not need to
  consider whether the index only needs to support equality queries, as opposed
  to greater/smaller than comparisons.

* There is no equivalent to
  [PostgreSQL's table access methods](https://www.postgresql.org/docs/13/tableam.html).
  Each table is clustered by a primary key index, which is implicitly stored in
  the table's B-tree.

By default, table rows (records / tuples) are stored in a B-tree keyed by
[rowid](https://sqlite.org/lang_createtable.html#rowid), an automatically
assigned 64-bit integer key. Effectively, these tables are clustered by rowid,
which acts as an implicit primary key. Opting out of this SQLite-specific
default requires appending
[`WITHOUT ROWID`](https://sqlite.org/withoutrowid.html) to the `CREATE TABLE`
instruction.

SQLite's [B-tree page format](https://sqlite.org/fileformat2.html#b_tree_pages)
has optimized special cases for tables clustered by rowid. This makes rowid the
most efficient [surrogate key](https://en.wikipedia.org/wiki/Surrogate_key)
implementation in SQLite. To make this optimization easier to use, any column
that is a primary key and has an `INTEGER` type is considered an alias for
rowid.

Each SQLite index
[is stored in a B-tree](https://sqlite.org/fileformat2.html#representation_of_sql_indices).
Each index entry is stored as a B-tree node whose key is made up of the record's
index key column values, followed by the record's primary key column values.

`WITHOUT ROWID` table indexes can include primary key columns without additional
storage costs. This is because indexes for `WITHOUT ROWID` tables enjoy
[a space optimization](https://sqlite.org/fileformat2.html#representation_of_sql_indices)
where columns in both the primary key and the index key are not stored twice in
B-tree nodes. Note that data in such tables cannot be recovered by `sql::Recovery`.


### Statement execution model {#query-model}

At [a very high level](https://www.sqlite.org/arch.html), SQLite compiles SQL
statements (often called *queries*) into bytecode executed by a virtual machine
called the VDBE, or [the bytecode engine](https://www.sqlite.org/opcode.html).
A compiled statement can be executed multiple times, amortizing the costs of
query parsing and planning. Chrome's SQLite abstraction layer makes it easy to
use compiled queries.

Assuming effective use of cached statements, the performance of a SQL statement
comes down to the *query plan* that SQLite generates for the statement. The
query plan is the sequence of B-tree accesses used to execute the statement,
which determines the number of B-tree pages touched.

The rest of this section summarizes the following SQLite documentation pages.

1. [query planner overview](https://www.sqlite.org/queryplanner.html)
2. [query optimizer overview](https://www.sqlite.org/optoverview.html)
3. [`EXPLAIN QUERY PLAN` output description](https://www.sqlite.org/eqp.html)

At a high level, a SQLite query plan is a sequence of **nested** loops, where
each loop iterates over the data in a B-tree. Each loop can use the current
record of the outer loops.

TODO: Complete this section. Cover joins, sorting, etc.

#### Getting SQLite's query plans

Ideally, the SQL schemas and statements used by Chrome features would be simple
enough that the query plans would be obvious to the reader.

When this isn't the case, the fastest way to get the query plan is to load the
schema in [the SQLite shell](https://sqlite.org/cli.html), and use
[`EXPLAIN QUERY PLAN`](https://www.sqlite.org/eqp.html).

The following command builds a SQLite shell that uses Chrome's build of SQLite,
and supports the `EXPLAIN QUERY PLAN` command.

```sh
autoninja -C out/Default sqlite_dev_shell
```

Inside the SQLite shell, the `.eqp on` directive automatically shows the results
of `EXPLAIN QUERY PLAN` for every SQL statement executed in the shell.


#### Query steps {#query-step-types}

Query steps are the building blocks of SQLite query plans. Each query step is
essentially a loop that iterates over the records in a B-tree. These loops
differ in terms of how many B-tree pages they touch, and how many records they
produce. This sub-section lists the types of steps implemented by SQLite.

##### Scans

Scans visit an entire (table or index) B-tree. For this reason, scans are almost
never acceptable in Chrome. Most of our features don't have limits on the amount
of stored data, so scans can result in an unbounded amount of I/O.

A *table scan* visits the entire table's B-tree.

A *covering index scan* visits an entire index B-tree, but doesn't access the
associated table B-tree.

SQLite doesn't have any special optimization for `COUNT(*)` queries. In other
words, SQLite does not track subtree sizes in its B-tree nodes.

Reviewers sometimes emphasize performance issues by calling the scans *full*
table scans and *full* index scans, where "full" references the fact that the
number of B-tree pages accessed is proportional to the entire data set stored on
disk.

TODO: Complete this section. Add examples in a way that doesn't make the section
overly long.

##### Searches

Searches access a subset of a (table or index) B-tree nodes. Searches limit the
amount of nodes they need to access based on query restrictions, such as terms
in the `WHERE` clause. Seeing a `SEARCH` in a query plan is not a guarantee of
performance. Searches can vary wildly in the amount of B-tree pages they need to
access.

One of the fastest possible searches is a *table search* that performs exactly
one B-tree lookup, and produces at most one record.

The other fastest possible search is a *covering index search* that also
performs one lookup, and produces at most one record.

TODO: Complete this section. Add examples in a way that doesn't make the section
overly long.


## General advice

The following pieces of advice usually come up in code reviews.


### Quickly iterating on SQL statements

[The SQLite shell](https://sqlite.org/cli.html) offers quick feedback for
converging on valid SQL statement syntax, and avoiding SQLite features that are
disabled in Chrome. In addition, the
[`EXPLAIN`](https://www.sqlite.org/lang_explain.html) and
[`EXPLAIN QUERY PLAN`](https://www.sqlite.org/eqp.html) statements show the
results of SQLite's query planner and optimizer, which are very helpful for
reasoning about the performance of complex queries. The SQLite shell directive
`.eqp on` automatically issues `EXPLAIN QUERY PLAN` for all future commands.


The following commands set up SQLite shells using Chrome's build of SQLite.

```sh
autoninja -C out/Default sqlite_shell sqlite_dev_shell
```

* `sqlite_shell` runs the SQLite build that we ship in Chrome. It offers the
  ground truth on whether a SQL statement can be used in Chrome code or not.
* `sqlite_dev_shell` enables the `EXPLAIN` and `EXPLAIN QUERY PLAN` statements,
  as well as a few features used by [Perfetto](https://perfetto.dev/)'s analysis
  tools.


### SQL style

SQLite queries are usually embedded as string literals in C++ code. The
advice here has the following goals.

1. Easy to read queries. The best defense against subtle bugs is making the
   queries very easy to read, so that any bugs become obvious at code review
   time. SQL string literals don't benefit from our code analysis
   infrastructure, so the only lines of defense against bugs are testing and
   code review.

2. Simplify crash debugging. We will always have a low volume of non-actionable
   crash reports, because Chrome runs on billions of devices, some of which have
   faulty RAM or processors.

3. No unnecessary performance overheads. The C++ optimizer doesn't understand
   SQL query literals, so the queries end up as written in the Chrome binary.
   Extra characters cost binary size, as well as CPU time (which turns into
   battery usage) during query parsing.

4. Match the embedding language (C++) style guide. This reduces the mental
   context switch overhead for folks who write and/or review C++ code that
   contains SQL.

Format statements like so.

```cc
  static constexpr char kOriginInfoSql[] =
      // clang-format off
      "CREATE TABLE origin_infos("
          "origin TEXT NOT NULL,"
          "last_modified INTEGER NOT NULL,"
          "secure INTEGER NOT NULL)";
  // clang-format on

  static constexpr char kInsertSql[] =
      // clang-format off
      "INSERT INTO infos(origin,last_modified,secure) "
          "VALUES(?,?,?)";
  // clang-format on

  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT origin,last_modified,secure FROM origins "
          "WHERE last_modified>? "
          "ORDER BY last_modified";
  // clang-format on
```

* [SQLite keywords](https://sqlite.org/lang_keywords.html) should use ALL CAPS.
  This makes SQL query literals easier to distinguish and search for.

* Identifiers, such as table and row names, should use snake_case.

* Identifiers, keywords, and parameter placeholders (`?`) should be separated by
  exactly one character. Separators may be spaces (` `), commas (`,`), or
  parentheses (`(`, `)`).

* Statement-ending semicolons (`;`) are omitted.

* SQL statements are stored in variables typed `static constexpr char[]`, or in
  string literals passed directly to methods.

* [`INSERT` statements](https://sqlite.org/lang_insert.html) should list all the
  table columns by name, in the same order as the corresponding `CREATE TABLE`
  statements.

* [`SELECT` statements](https://sqlite.org/lang_select.html) should list the
  desired table columns by name, in the same order as the corresponding
  `CREATE TABLE` statements. `SELECT *` is strongly discouraged, at least until
  we have schema checks on database opens.

* [`SELECT` statements](https://sqlite.org/lang_select.html) that retrieve more
  than one row should include an
  [`ORDER BY` clause](https://sqlite.org/lang_select.html#the_order_by_clause)
  to clarify the implicit ordering.
  * SELECTs whose outer loop is a table search or table scan implicitly order
    results by [rowid](https://sqlite.org/lang_createtable.html#rowid) or, in
    the case of [`WITHOUT ROWID`](https://sqlite.org/withoutrowid.html) tables,
    by the table's primary key.
  * SELECTs whose outer loop is an index scan or index search order results
    according to that index.

* [`CREATE INDEX` statements](https://sqlite.org/lang_createindex.html) should
  immediately follow the
  [`CREATE TABLE` statement](https://sqlite.org/lang_createtable.html) for the
  indexed table.

* Explicit `CREATE UNIQUE INDEX` statements should be preferred to
  [`UNIQUE` constraints on `CREATE TABLE`](https://sqlite.org/lang_createtable.html#unique_constraints).

* Values must either be embedded in the SQL statement string literal, or bound
  using [parameters](https://www.sqlite.org/lang_expr.html#varparam).

* Parameter placeholders should always use the `?` syntax. Alternative syntaxes,
  such as `?NNN` or `:AAAA`, have few benefits in a codebase where the `Bind`
  statements are right next to the queries, and are less known to readers.

* SQL statements should be embedded in C++ as string literals. The `char[]` type
  makes it possible for us to compute query length at compile time in the
  future. The `static` and `constexpr` qualifiers both ensure optimal code
  generation.

* Do not execute multiple SQL statements (e.g., by calling `Step()` or `Run()`
  on `sql::Statement`) on the same C++ line. It's difficult to get more than
  line numbers from crash reports' stack traces.


### Schema style

Identifiers (table / index / column names and aliases) must not be
[current SQLite keywords](https://sqlite.org/lang_keywords.html). Identifiers
may not start with the `sqlite_` prefix, to avoid conflicting with the name of a
[SQLite internal schema object](https://www.sqlite.org/fileformat2.html#storage_of_the_sql_database_schema).

Column types should only be one of the the SQLite storage types (`INTEGER`,
`REAL`, `TEXT`, `BLOB`), so readers can avoid reasoning about SQLite's type
affinity.

Columns that will store boolean values should have the `INTEGER` type.

Columns that will store `base::Time` values should have the `INTEGER` type.
Values should be serialized using `sql::Statement::BindTime()` and deserialized
using `sql::Statement::ColumnTime()`.

Column types should not include information ignored by SQLite, such as numeric
precision or scale specifiers, or string length specifiers.

Columns should have
[`NOT NULL` constraints](https://sqlite.org/lang_createtable.html#not_null_constraints)
whenever possible. This saves maintainers from having to reason about the less
intuitive cases of [`NULL` handling](https://sqlite.org/nulls.html).

`NOT NULL` constraints must be explicitly stated in column definitions that
include `PRIMARY KEY` specifiers. For historical reasons, SQLite
[allows NULL primary keys](https://sqlite.org/lang_createtable.html#the_primary_key)
in most cases.  When a table's primary key is composed of multiple columns,
each column's definition should have a `NOT NULL` constraint.

Columns should avoid `DEFAULT` values. Columns that have `NOT NULL` constraints
and lack a `DEFAULT` value are easier to review and maintain, as SQLite takes
over the burden of checking that `INSERT` statements aren't missing these
columns.

Surrogate primary keys should use the column type `INTEGER PRIMARY KEY`, to take
advantage of SQLite's rowid optimizations.
[`AUTOINCREMENT`](https://www.sqlite.org/autoinc.html) should only be used where
primary key reuse would be unacceptable.


### Discouraged features

SQLite exposes a vast array of functionality via SQL statements. The following
features are not a good match for SQL statements used by Chrome feature code.

#### PRAGMA statements {#no-pragmas}

[`PRAGMA` statements](https://www.sqlite.org/pragma.html) should never be used
directly. Chrome's SQLite abstraction layer should be modified to support the
desired effects instead.

Direct `PRAGMA` use limits our ability to customize and secure our SQLite build.
`PRAGMA` statements may turn on code paths with less testing / fuzzing coverage.
Furthermore, some `PRAGMA` statements invalidate previously compiled queries,
reducing the efficiency of Chrome's compiled query cache.

#### Foreign key constraints {#no-foreign-keys}

[SQL foreign key constraints](https://sqlite.org/foreignkeys.html) should not be
used. All data validation should be performed using explicit `SELECT` statements
(generally wrapped as helper methods) inside transactions. Cascading deletions
should be performed using explicit `DELETE` statements inside transactions.

Chrome features cannot rely on foreign key enforcement, due to the
possibility of data corruption. Furthermore, foreign key constraints make it
more difficult to reason about system behavior (Chrome feature code + SQLite)
when the database gets corrupted. Foreign key constraints also make it more
difficult to reason about query performance.

As a result, foreign key constraints are not enforced on SQLite databases
opened with Chrome's `sql::Database` infrastructure.

After
[WebSQL](https://www.w3.org/TR/webdatabase/) is removed from Chrome, we plan
to disable SQLite's foreign key support using
[SQLITE_OMIT_FOREIGN_KEY](https://sqlite.org/compile.html#omit_foreign_key).

#### CHECK constraints {#no-checks}

[SQL CHECK constraints](https://sqlite.org/lang_createtable.html#check_constraints)
should not be used, for the same reasons as foreign key constraints. The
equivalent checks should be performed in C++, typically using `DCHECK`.

After
[WebSQL](https://www.w3.org/TR/webdatabase/) is removed from Chrome, we plan
to disable SQLite's CHECK constraint support using
[SQLITE_OMIT_CHECK](https://sqlite.org/compile.html#omit_check).

#### Triggers {#no-triggers}

[SQL triggers](https://sqlite.org/lang_createtrigger.html) should not be used.

Triggers significantly increase the difficulty of reviewing and maintaining
Chrome features that use them.

Triggers are not executed on SQLite databases opened with Chrome's
`sql::Database` infrastructure. This is intended to steer feature developers
away from the discouraged feature.

After [WebSQL](https://www.w3.org/TR/webdatabase/) is removed from Chrome, we
plan to disable SQLite's trigger support using
[SQLITE_OMIT_TRIGGER](https://sqlite.org/compile.html#omit_trigger).

#### Common Table Expressions {#no-ctes}

[SQL Common Table Expressions (CTEs)](https://sqlite.org/lang_with.html) should
not be used. Chrome's SQL schemas and queries should be simple enough that
the factoring afforded by
[ordinary CTEs](https://sqlite.org/lang_with.html#ordinary_common_table_expressions)
is not necessary.
[Recursive CTEs](https://sqlite.org/lang_with.html#recursive_common_table_expressions)
should be implemented in C++.

Common Table Expressions do not open up any query optimizations that would not
be available otherwise, and make it more difficult to review / analyze queries.

#### Views {#no-views}

SQL views, managed by the
[`CREATE VIEW` statement](https://www.sqlite.org/lang_createview.html) and the
[`DROP VIEW` statement](https://www.sqlite.org/lang_dropview.html), should not
be used. Chrome's SQL schemas and queries should be simple enough that the
factoring afforded by views is not necessary.

Views are syntactic sugar, and do not open up any new SQL capabilities. SQL
statements on views are more difficult to understand and maintain, because of
the extra layer of indirection.

Access to views is disabled by default for SQLite databases opened with Chrome's
`sql::Database` infrastructure. This is intended to steer feature developers
away from the discouraged feature.

After
[WebSQL](https://www.w3.org/TR/webdatabase/) is removed from Chrome, we plan
to disable SQLite's VIEW support using
[SQLITE_OMIT_VIEW](https://www.sqlite.org/compile.html#omit_view).

#### Double-quoted string literals {#no-double-quoted-strings}

String literals should always be single-quoted. That being said, string literals
should be rare in Chrome code, because any user input must be injected using
statement parameters and the `Statement::Bind*()` methods.

Double-quoted string literals are non-standard SQL syntax. The SQLite authors
[currently consider this be a misfeature](https://www.sqlite.org/quirks.html#double_quoted_string_literals_are_accepted).

SQLite support for double-quoted string literals is disabled for databases
opened with Chrome's `sql::Database` infrastructure. This is intended to steer
feature developers away from this discouraged feature.

After
[WebSQL](https://www.w3.org/TR/webdatabase/) is removed from Chrome, we plan
to disable SQLite's support for double-quoted string literals using
[SQLITE_DQS=0](https://www.sqlite.org/compile.html#dqs).

#### Compound SELECT statements {#no-compound-queries}

[Compound SELECT statements](https://www.sqlite.org/lang_select.html#compound_select_statements)
should not be used. Such statements should be broken down into
[simple SELECT statements](https://www.sqlite.org/lang_select.html#simple_select_processing),
and the operators `UNION`, `UNION ALL`, `INTERSECT` and `EXCEPT` should be
implemented in C++.

A single compound SELECT statement is more difficult to review and properly
unit-test than the equivalent collection of simple SELECT statements.
Furthermore, the compound SELECT statement operators can be implemented more
efficiently in C++ than in SQLite's bytecode interpreter (VDBE).

After
[WebSQL](https://www.w3.org/TR/webdatabase/) is removed from Chrome, we plan
to disable SQLite's compound SELECT support using
[SQLITE_OMIT_COMPOUND_SELECT](https://www.sqlite.org/compile.html#omit_compound_select).

#### Built-in functions {#no-builtin-functions}

SQLite's [built-in functions](https://sqlite.org/lang_corefunc.html) should be
only be used in SQL statements where they unlock significant performance
improvements. Chrome features should store data in a format that leaves the most
room for query optimizations, and perform any necessary transformations after
reading / before writing the data.

* [Aggregation functions](https://sqlite.org/lang_aggfunc.html) are best
  replaced with C++ code that iterates over rows and computes the desired
  results.
* [Date and time functions](https://sqlite.org/lang_datefunc.html) are best
  replaced by `base::Time` functionality.
* String-processing functions, such as
  [`printf()`](https://sqlite.org/printf.html) and `trim()` are best replaced
  by C++ code that uses the helpers in `//base/strings/`.
* Wrappers for [SQLite's C API](https://sqlite.org/c3ref/funclist.html), such as
  `changes()`, `last_insert_rowid()`, and `total_changes()`, are best replaced
  by functionality in `sql::Database` and `sql::Statement`.
* SQLite-specific functions, such as  `sqlite_source_id()` and
  `sqlite_version()` should not be necessary in Chrome code, and may suggest a
  problem in the feature's design.

[Math functions](https://sqlite.org/lang_mathfunc.html) and
[Window functions](https://sqlite.org/windowfunctions.html#biwinfunc) are
disabled in Chrome's SQLite build.

#### ATTACH DATABASE statements

[`ATTACH DATABASE` statements](https://www.sqlite.org/lang_attach.html) should
be used thoughtfully. Each Chrome feature should store its data in a single database.
Chrome code should not assume that transactions across multiple databases are
atomic.

### Disabled features

We aim to disable SQLite features that should not be used in Chrome, subject to
the constraint of keeping WebSQL's feature set stable. We currently disable all
new SQLite features, to avoid expanding the attack surface exposed to WebSQL.
This stance may change once WebSQL is removed from Chrome.

The following SQLite features have been disabled in Chrome.

#### JSON

Chrome features should prefer
[procotol buffers](https://developers.google.com/protocol-buffers) to JSON for
on-disk (persistent) serialization of extensible structured data.

Chrome features should store the values used by indexes directly in their own
columns, instead of relying on
[SQLite's JSON support](https://www.sqlite.org/json1.html).

#### UPSERT

[SQLite's UPSERT implementation](https://www.sqlite.org/lang_UPSERT.html) has
been disabled in order to avoid increasing WebSQL's attack surface. UPSERT is
disabled using the `SQLITE_OMIT_UPSERT` macro, which is not currently included
in [the SQLite compile-time option list](https://www.sqlite.org/compile.html),
but exists in the source code.

We currently think that the new UPSERT functionality is not essential to
implementing Chrome features efficiently. An example where UPSERT is necessary
for the success of a Chrome feature would likely get UPSERT enabled.

#### Window functions

[Window functions](https://sqlite.org/windowfunctions.html#biwinfunc) have been
disabled primarily because they cause a significant binary size increase, which
leads to a corresponding large increase in the attack surface exposed to WebSQL.

Window functions increase the difficulty of reviewing and maintaining the Chrome
features that use them, because window functions add complexity to the mental
model of query performance.

We currently think that this maintenance overhead of window functions exceeds
any convenience and performance benefits (compared to simpler queries
coordinated in C++).

#### Virtual tables {#no-virtual-tables}

[`CREATE VIRTUAL TABLE` statements](https://www.sqlite.org/vtab.html) are
disabled. The desired functionality should be implemented in C++, and access
storage using standard SQL statements.

Virtual tables are [SQLite's module system](https://www.sqlite.org/vtab.html).
SQL statements on virtual tables are essentially running arbitrary code, which
makes them very difficult to reason about and maintain. Furthermore, the virtual
table implementations don't receive the same level of fuzzing coverage as the
SQLite core.

Chrome's SQLite build has virtual table functionality reduced to the minimum
needed to support an internal feature.
[SQLite's run-time loading mechanism](https://www.sqlite.org/loadext.html) is
disabled, and most
[built-in virtual tables](https://www.sqlite.org/vtablist.html) are disabled as
well.

Ideally we would disable SQLite's virtual table support using
[SQLITE_OMIT_VIRTUALTABLE](https://sqlite.org/compile.html#omit_virtualtable)
now that [WebSQL](https://www.w3.org/TR/webdatabase/) has been removed from
Chrome, but virtual table support is required to use SQLite's
[built-in corruption recovery module](https://www.sqlite.org/recovery.html). The
[SQLITE_DBPAGE virtual table](https://www.sqlite.org/dbpage.html) is also
enabled only for corruption recovery and should not be used in Chrome.
