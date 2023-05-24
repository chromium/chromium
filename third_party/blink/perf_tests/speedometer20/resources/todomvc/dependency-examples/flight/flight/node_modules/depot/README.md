## depot.js

[![build status](https://secure.travis-ci.org/mkuklis/depot.js.png)](http://travis-ci.org/mkuklis/depot.js)

![depot.js](http://oi45.tinypic.com/xoiq7l.jpg)


## Description

**depot.js** is a namespaced [localStorage](http://diveintohtml5.info/storage.html) wrapper with a simple API.
There are [other](http://brian.io/lawnchair/) [tools](https://github.com/marcuswestin/store.js/) out there but none
of them had what I was looking for.


## Setup

depot.js should work well with CommonJS and AMD loaders.
If loaders are not present depot.js will attach itself to the current context (window) when loaded via `<script src="depot.min.js"></script>`.

depot.js is also a [bower](https://github.com/twitter/bower) [component](http://sindresorhus.com/bower-components/) so you should be able to install it by running:

`bower install depot`

or if you already have a bower based project you can add depot.js to your dependency list in `component.json`

```js
 "dependencies": {
    ...
    "depot": "0.x.x"
    ...
  }
```


## Dependencies

depot.js does not depend on any other libraries however if you plan to support older browsers you will need to include [ES5-shim](https://github.com/kriskowal/es5-shim).

If you plan to run it on browsers that don't support [localStorage](http://diveintohtml5.info/storage.html) you may try to include [storage polyfill](https://gist.github.com/remy/350433).

## API

+ save(record)

+ updateAll(hash)

+ update(hash)

+ find(hash | function)

+ all()

+ destroy(id | record)

+ destroyAll(none | hash | function)

+ get(id)

+ size()

##Usage

####Define new store

```js
var todoStore = depot('todos');
```

####Add new records

`_id` property will be generated and attached to each new record:

```js
todoStore.save({ title: "todo1" });
todoStore.save({ title: "todo2", completed: true });
todoStore.save({ title: "todo3", completed: true });
```

####Update all records

```js
todoStore.updateAll({ completed: false });
```

####Return all records

```js
todoStore.all(); // [{ id: 1, title "todo1" }, {id: 2, title: todo2 }]
```

####Find records

* find based on given criteria

```js
todoStore.find({ completed: true }); // [{ id: 2, title: "todo2" }, { id: 3, title: "todo3" }]
```

* find based on given function

```js
todoStore.find(function (record) {
  return record.completed && record.title == "todo3";
}); // [{ id: 3, title: "todo3" }]
```


####Return single record by id

```js
todoStore.get(1); // { id: 1, title: "todo1" }
```

####Destroy single record

* by record id

```js
todoStore.destroy(1);
```

* by record object

```js
todoStore.destroy(todo);
```

####Destroy all records

* destroy all

```js
todoStore.destroyAll();
```

* destroy by given criteria

```js
todoStore.destroyAll({ completed: true });
```

* destroy by given function

```js
todoStore.destroyAll(function (record) {
  return record.completed && record.title == "todo3";
});
```

##Options

You can pass a second parameter to depot.js with additional options.

```js
var todoStore = depot("todos", options);
```

### Available options:

+ idAttribute - used to override record id property (default: `_id`)

```js
var todoStore = depot("todos", { idAttribute: 'id' });
```

+ storageAdaptor - used to override storage type (default: `localStorage`)

```js
var todoStore = depot('todos', { storageAdaptor: sessionStorage });
```


##Contributors:

* [@mkuklis](http://github.com/mkuklis)
* [@scttnlsn](http://github.com/scttnlsn)
* [@chrispitt](http://github.com/chrispitt)
* [@simonsmith](http://github.com/simonsmith)
* [@mdlawson](http://github.com/mdlawson)
* [@jdbartlett](http://github.com/jdbartlett)

##License:
<pre>
The MIT License
</pre>
