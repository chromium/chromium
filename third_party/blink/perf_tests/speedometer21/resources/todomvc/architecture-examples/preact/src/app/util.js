export function uuid() {
    let uuid = '';
    for (let i = 0; i < 32; i++) {
        let random = Math.random() * 16 | 0;
        if (i === 8 || i === 12 || i === 16 || i === 20) {
            uuid += '-';
        }
        uuid += (i === 12 ? 4 : (i === 16 ? (random & 3 | 8) : random)).toString(16);
    }
    return uuid;
}

export function pluralize(count, word) {
    return count === 1 ? word : word + 's';
}

export function store(namespace, data) {
    // if (data) return localStorage[namespace] = JSON.stringify(data);

    // let store = localStorage[namespace];
    // return store && JSON.parse(store) || [];
    return [];
}
