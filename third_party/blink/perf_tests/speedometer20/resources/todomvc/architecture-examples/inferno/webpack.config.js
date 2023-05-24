const webpack = require('webpack');

module.exports = {
    entry: './src/index.js',
    output: {
        path: './dist',
        publicPath: 'dist',
        filename: 'bundle.js'
    },
    module: {
        loaders: [{
            test: /\.jsx?$/,
            exclude: /node_modules/,
            loader: 'babel',
            query: {
                presets: ['es2015-loose', 'stage-0'],
                plugins: ['syntax-jsx', 'inferno']
            }
        }]
    },
    plugins: [
        new webpack.DefinePlugin({
            'process.env': {
                'NODE_ENV': JSON.stringify('production')
            }
        }),
    ],
    devServer: {
        port: process.env.PORT || 8000
    }
};
