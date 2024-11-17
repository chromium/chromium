var testCases = [
    {
        name: 'CreateSimple',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true}); },
            function(helper) { helper.getFile('/', 'b', {create:true}); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'},
        ],
    },
    {
        name: 'CreateNested',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true}); },
            function(helper) { helper.getDirectory('/a', 'b', {create:true}); },
            function(helper) { helper.getDirectory('/a/b', 'c', {create:true}); },
            function(helper) { helper.getDirectory('/a/b/c', 'd', {create:true}); },
            function(helper) { helper.getFile('/a/b/c/d', 'e', {create:true}); },
        ],
        postcondition: [
            {fullPath:'/a/b/c/d/e'},
        ],
    },
    {
        name: 'CreateNestedWithAbsolutePath',
        precondition: [
            {fullPath:'/dummy', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.getDirectory('/dummy', '/a', {create:true}); },
            function(helper) { helper.getDirectory('/dummy', '/a/b', {create:true}); },
            function(helper) { helper.getDirectory('/dummy', '/a/b/c', {create:true}); },
            function(helper) { helper.getDirectory('/dummy', '/a/b/c/d', {create:true}); },
            function(helper) { helper.getFile('/dummy', '/a/b/c/d/e', {create:true}); }
        ],
        postcondition: [
            {fullPath:'/dummy', isDirectory:true},
            {fullPath:'/a/b/c/d/e'},
        ],
    },
    {
        name: 'CreateNestedWithRelativePath',
        precondition: [
            {fullPath:'/a', isDirectory:true},
        ],
        tests: [
            function(helper) { helper.getDirectory('/a', './b', {create:true}); },
            function(helper) { helper.getDirectory('/a', '../b', {create:true}); },
            function(helper) { helper.getDirectory('/a', '../../b/c', {create:true}); },
            function(helper) { helper.getDirectory('/a', '/a/../../d', {create:true}); },
            function(helper) { helper.getDirectory('/a', '/a/../../b/./c/../../../../../e', {create:true}); },
            function(helper) { helper.getDirectory('/a', '/a/../.../g', {create:true}, 'NotFoundError'); },
            function(helper) { helper.getFile('/a', './b.txt', {create:true}); },
            function(helper) { helper.getFile('/a', '../b.txt', {create:true}); },
            function(helper) { helper.getFile('/a', '../../b/c.txt', {create:true}); },
            function(helper) { helper.getFile('/a', '/a/../../d.txt', {create:true}); },
            function(helper) { helper.getFile('/a', '/a/../../b/./c/../../../../../e.txt', {create:true}); },
            function(helper) { helper.getFile('/a', '/a/../.../g.txt', {create:true}, 'NotFoundError'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/a/b', isDirectory:true},
            {fullPath:'/b', isDirectory:true},
            {fullPath:'/b/c', isDirectory:true},
            {fullPath:'/d', isDirectory:true},
            {fullPath:'/e', isDirectory:true},
            {fullPath:'/f', nonexistent:true},
            {fullPath:'/a/f', nonexistent:true},
            {fullPath:'/g', nonexistent:true},
            {fullPath:'/a/g', nonexistent:true},
            {fullPath:'/a/b.txt'},
            {fullPath:'/b.txt'},
            {fullPath:'/b/c.txt'},
            {fullPath:'/d.txt'},
            {fullPath:'/e.txt'},
            {fullPath:'/f.txt', nonexistent:true},
            {fullPath:'/a/f.txt', nonexistent:true},
            {fullPath:'/g.txt', nonexistent:true},
            {fullPath:'/a/g.txt', nonexistent:true},
        ],
    },
    {
        name: 'GetExistingEntry',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true}); },
            function(helper) { helper.getFile('/', 'b', {create:true}); },
            function(helper) { helper.getDirectory('/', 'a'); },
            function(helper) { helper.getFile('/', 'b'); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'},
        ],
    },
    {
        name: 'GetNonExistent',
        precondition: [ ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {}, 'NotFoundError'); },
            function(helper) { helper.getFile('/', 'b', {}, 'NotFoundError'); },
            function(helper) { helper.getDirectory('/', '/nonexistent/a', {create:true}, 'NotFoundError'); },
            function(helper) { helper.getFile('/', '/nonexistent/b', {create:true}, 'NotFoundError'); }
        ],
        postcondition: [ ],
    },
    {
        name: 'GetFileForDirectory',
        precondition: [
            {fullPath:'/a', isDirectory:true}
        ],
        tests: [
            function(helper) { helper.getFile('/', 'a', {}, 'TypeMismatchError'); },
            function(helper) { helper.getFile('/', '/a', {}, 'TypeMismatchError'); },
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true}
        ],
    },
    {
        name: 'GetDirectoryForFile',
        precondition: [
            {fullPath:'/a'}
        ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {}, 'TypeMismatchError'); },
            function(helper) { helper.getDirectory('/', '/a', {}, 'TypeMismatchError'); },
        ],
        postcondition: [
            {fullPath:'/a'}
        ],
    },
    {
        name: 'CreateWithExclusive',
        precondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'}
        ],
        tests: [
            function(helper) { helper.getDirectory('/', 'a', {create:true, exclusive:true}, 'InvalidModificationError'); },
            function(helper) { helper.getFile('/', 'b', {create:true, exclusive:true}, 'InvalidModificationError'); }
        ],
        postcondition: [
            {fullPath:'/a', isDirectory:true},
            {fullPath:'/b'}
        ],
    },
];
